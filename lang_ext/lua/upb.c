/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Joshua Haberman.  See LICENSE for details.
 *
 * A Lua extension for upb.
 */

#include <stdlib.h>
#include "lauxlib.h"
#include "upb_def.h"

void lupb_pushstring(lua_State *L, upb_string *str) {
  lua_pushlstring(L, upb_string_getrobuf(str), upb_string_len(str));
}

/* object cache ***************************************************************/

// We cache all the lua objects (userdata) we vend in a weak table, indexed by
// the C pointer of the object they are caching.

typedef void (*lupb_cb)(void *cobj);

static void lupb_nop(void *foo) {
  (void)foo;
}

static void lupb_cache_getorcreate(lua_State *L, void *cobj, const char *type,
                                   lupb_cb ref, lupb_cb unref) {
  // Lookup our cache in the registry (we don't put our objects in the registry
  // directly because we need our cache to be a weak table).
  lua_getfield(L, LUA_REGISTRYINDEX, "upb.objcache");
  assert(!lua_isnil(L, -1));  // Should have been created by luaopen_upb.
  lua_pushlightuserdata(L, cobj);
  lua_rawget(L, -2);
  // Stack: objcache, cached value.
  if (lua_isnil(L, -1)) {
    // Remove bad cached value and push new value.
    lua_pop(L, 1);
    // We take advantage of the fact that all of our objects are currently a
    // single pointer, and thus have the same layout.
    void **obj = lua_newuserdata(L, sizeof(void*));
    *obj = cobj;
    luaL_getmetatable(L, type);
    assert(!lua_isnil(L, -1));  // Should have been created by luaopen_upb.
    lua_setmetatable(L, -2);

    // Set it in the cache.
    lua_pushlightuserdata(L, cobj);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
    ref(cobj);
  } else {
    unref(cobj);
  }
  lua_insert(L, -2);
  lua_pop(L, 1);
}


/* lupb_def *******************************************************************/

// All the def types share the same C layout, even though they are different Lua
// types with different metatables.
typedef struct {
  upb_def *def;
} lupb_def;

static void lupb_def_unref(void *cobj) {
  upb_def_unref((upb_def*)cobj);
}

static void lupb_def_getorcreate(lua_State *L, upb_def *def) {
  const char *type_name;
  switch(def->type) {
    case UPB_DEF_MSG:
      type_name = "upb.msgdef";
      break;
    case UPB_DEF_ENUM:
      type_name = "upb.enumdef";
      break;
    default:
      luaL_error(L, "unknown deftype %d", def->type);
      type_name = NULL;  // Placate the compiler.
  }
  return lupb_cache_getorcreate(L, def, type_name, lupb_nop, lupb_def_unref);
}

// msgdef

static upb_msgdef *lupb_msgdef_check(lua_State *L, int narg) {
  lupb_def *ldef = luaL_checkudata(L, narg, "upb.msgdef");
  return upb_downcast_msgdef(ldef->def);
}

static int lupb_msgdef_gc(lua_State *L) {
  lupb_def *ldef = luaL_checkudata(L, 1, "upb.msgdef");
  upb_def_unref(ldef->def);
  return 0;
}

static void lupb_fielddef_getorcreate(lua_State *L, upb_fielddef *f);

static int lupb_msgdef_name(lua_State *L) {
  upb_msgdef *m = lupb_msgdef_check(L, 1);
  lupb_pushstring(L, m->base.fqname);
  return 1;
}

static int lupb_msgdef_fieldbyname(lua_State *L) {
  upb_msgdef *m = lupb_msgdef_check(L, 1);
  size_t len;
  const char *name = luaL_checklstring(L, 2, &len);
  upb_string namestr = UPB_STACK_STRING_LEN(name, len);
  upb_fielddef *f = upb_msgdef_ntof(m, &namestr);
  if (f) {
    lupb_fielddef_getorcreate(L, f);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int lupb_msgdef_fieldbynum(lua_State *L) {
  upb_msgdef *m = lupb_msgdef_check(L, 1);
  int num = luaL_checkint(L, 2);
  upb_fielddef *f = upb_msgdef_itof(m, num);
  if (f) {
    lupb_fielddef_getorcreate(L, f);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static const struct luaL_Reg lupb_msgdef_mm[] = {
  {"__gc", lupb_msgdef_gc},
  {NULL, NULL}
};

static const struct luaL_Reg lupb_msgdef_m[] = {
  {"fieldbyname", lupb_msgdef_fieldbyname},
  {"fieldbynum", lupb_msgdef_fieldbynum},
  {"name", lupb_msgdef_name},
  {NULL, NULL}
};

// enumdef

static upb_enumdef *lupb_enumdef_check(lua_State *L, int narg) {
  lupb_def *ldef = luaL_checkudata(L, narg, "upb.enumdef");
  return upb_downcast_enumdef(ldef->def);
}

static int lupb_enumdef_gc(lua_State *L) {
  upb_enumdef *e = lupb_enumdef_check(L, 1);
  upb_def_unref(UPB_UPCAST(e));
  return 0;
}

static int lupb_enumdef_name(lua_State *L) {
  upb_enumdef *e = lupb_enumdef_check(L, 1);
  lupb_pushstring(L, e->base.fqname);
  return 1;
}

static const struct luaL_Reg lupb_enumdef_mm[] = {
  {"__gc", lupb_enumdef_gc},
  {NULL, NULL}
};

static const struct luaL_Reg lupb_enumdef_m[] = {
  {"name", lupb_enumdef_name},
  {NULL, NULL}
};


/* lupb_fielddef **************************************************************/

typedef struct {
  upb_fielddef *field;
} lupb_fielddef;

static void lupb_fielddef_ref(void *cobj) {
  upb_def_ref(UPB_UPCAST(((upb_fielddef*)cobj)->msgdef));
}

static void lupb_fielddef_getorcreate(lua_State *L, upb_fielddef *f) {
  lupb_cache_getorcreate(L, f, "upb.fielddef", lupb_fielddef_ref, lupb_nop);
}

static lupb_fielddef *lupb_fielddef_check(lua_State *L, int narg) {
  return luaL_checkudata(L, narg, "upb.fielddef");
}

static int lupb_fielddef_index(lua_State *L) {
  lupb_fielddef *f = lupb_fielddef_check(L, 1);
  const char *str = luaL_checkstring(L, 2);
  if (strcmp(str, "name") == 0) {
    lupb_pushstring(L, f->field->name);
  } else if (strcmp(str, "number") == 0) {
    lua_pushinteger(L, f->field->number);
  } else if (strcmp(str, "type") == 0) {
    lua_pushinteger(L, f->field->type);
  } else if (strcmp(str, "label") == 0) {
    lua_pushinteger(L, f->field->label);
  } else if (strcmp(str, "def") == 0) {
    upb_def_ref(f->field->def);
    lupb_def_getorcreate(L, f->field->def);
  } else if (strcmp(str, "msgdef") == 0) {
    upb_def_ref(UPB_UPCAST(f->field->msgdef));
    lupb_def_getorcreate(L, UPB_UPCAST(f->field->msgdef));
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int lupb_fielddef_gc(lua_State *L) {
  lupb_fielddef *lfielddef = lupb_fielddef_check(L, 1);
  upb_def_unref(UPB_UPCAST(lfielddef->field->msgdef));
  return 0;
}

static const struct luaL_Reg lupb_fielddef_mm[] = {
  {"__gc", lupb_fielddef_gc},
  {"__index", lupb_fielddef_index},
  {NULL, NULL}
};


/* lupb_symtab ****************************************************************/

typedef struct {
  upb_symtab *symtab;
} lupb_symtab;

// Inherits a ref on the symtab.
// Checks that narg is a proper lupb_symtab object.  If it is, leaves its
// metatable on the stack for cache lookups/updates.
lupb_symtab *lupb_symtab_check(lua_State *L, int narg) {
  return luaL_checkudata(L, narg, "upb.symtab");
}

static int lupb_symtab_gc(lua_State *L) {
  lupb_symtab *s = lupb_symtab_check(L, 1);
  upb_symtab_unref(s->symtab);
  return 0;
}

static void lupb_symtab_unref(void *cobj) {
  upb_symtab_unref((upb_symtab*)cobj);
}

static int lupb_symtab_lookup(lua_State *L) {
  lupb_symtab *s = lupb_symtab_check(L, 1);
  size_t len;
  const char *name = luaL_checklstring(L, 2, &len);
  upb_string namestr = UPB_STACK_STRING_LEN(name, len);
  upb_def *def = upb_symtab_lookup(s->symtab, &namestr);
  if (def) {
    lupb_def_getorcreate(L, def);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int lupb_symtab_getdefs(lua_State *L) {
  lupb_symtab *s = lupb_symtab_check(L, 1);
  upb_deftype_t type = luaL_checkint(L, 2);
  int count;
  upb_def **defs = upb_symtab_getdefs(s->symtab, &count, type);

  // Create the table in which we will return the defs.
  lua_createtable(L, count, 0);
  for (int i = 0; i < count; i++) {
    upb_def *def = defs[i];
    lua_pushnumber(L, i + 1);  // 1-based array.
    lupb_def_getorcreate(L, def);
    // Add it to our return table.
    lua_settable(L, -3);
  }
  free(defs);
  return 1;
}

static int lupb_symtab_add_descriptorproto(lua_State *L) {
  lupb_symtab *s = lupb_symtab_check(L, 1);
  upb_symtab_add_descriptorproto(s->symtab);
  return 0;  // No args to return.
}

static const struct luaL_Reg lupb_symtab_m[] = {
  {"add_descriptorproto", lupb_symtab_add_descriptorproto},
  //{"addfds", lupb_symtab_addfds},
  {"getdefs", lupb_symtab_getdefs},
  {"lookup", lupb_symtab_lookup},
  //{"resolve", lupb_symtab_resolve},
  {NULL, NULL}
};

static const struct luaL_Reg lupb_symtab_mm[] = {
  {"__gc", lupb_symtab_gc},
  {NULL, NULL}
};


/* lupb toplevel **************************************************************/

static int lupb_symtab_new(lua_State *L) {
  upb_symtab *s = upb_symtab_new();
  lupb_cache_getorcreate(L, s, "upb.symtab", lupb_nop, lupb_symtab_unref);
  return 1;
}

static const struct luaL_Reg lupb_toplevel_m[] = {
  {"symtab", lupb_symtab_new},
  {NULL, NULL}
};

// Register the given type with the given methods and metamethods.
static void lupb_register_type(lua_State *L, const char *name,
                               const luaL_Reg *m, const luaL_Reg *mm) {
  luaL_newmetatable(L, name);
  luaL_register(L, NULL, mm);  // Register all mm in the metatable.
  lua_createtable(L, 0, 0);
  if (m) {
    // Methods go in the mt's __index method.  This implies that you can't
    // implement __index and also set methods yourself.
    luaL_register(L, NULL, m);
    lua_setfield(L, -2, "__index");  
  }
  lua_pop(L, 1);  // The mt.
}

int luaopen_upb(lua_State *L) {
  lupb_register_type(L, "upb.msgdef", lupb_msgdef_m, lupb_msgdef_mm);
  lupb_register_type(L, "upb.enumdef", lupb_enumdef_m, lupb_enumdef_mm);
  lupb_register_type(L, "upb.fielddef", NULL, lupb_fielddef_mm);
  lupb_register_type(L, "upb.symtab", lupb_symtab_m, lupb_symtab_mm);

  // Create our object cache.  TODO: need to make this table weak!
  lua_createtable(L, 0, 0);
  lua_createtable(L, 0, 1);  // Cache metatable.
  lua_pushstring(L, "v");    // Values are weak.
  lua_setfield(L, -2, "__mode");
  lua_setfield(L, LUA_REGISTRYINDEX, "upb.objcache");

  luaL_register(L, "upb", lupb_toplevel_m);
  return 1;  // Return package table.
}
load(
    "//bazel:upb_proto_library.bzl",
    "upb_proto_library",
    "upb_proto_reflection_library",
)

licenses(["notice"])

proto_library(
    name = "benchmark_descriptor_proto",
    srcs = ["descriptor.proto"],
)

upb_proto_library(
    name = "benchmark_descriptor_upb_proto",
    deps = [":benchmark_descriptor_proto"],
)

upb_proto_reflection_library(
    name = "benchmark_descriptor_upb_proto_reflection",
    deps = [":benchmark_descriptor_proto"],
)

upb_proto_reflection_library(
    name = "ads_upb_proto_reflection",
    deps = ["@com_google_googleapis//:ads_proto"],
)

cc_proto_library(
    name = "benchmark_descriptor_cc_proto",
    deps = [":benchmark_descriptor_proto"],
)

proto_library(
    name = "benchmark_descriptor_sv_proto",
    srcs = ["descriptor_sv.proto"],
)

cc_proto_library(
    name = "benchmark_descriptor_sv_cc_proto",
    deps = [":benchmark_descriptor_sv_proto"],
)

cc_binary(
    name = "benchmark",
    testonly = 1,
    srcs = ["benchmark.cc"],
    deps = [
        ":ads_upb_proto_reflection",
        ":benchmark_descriptor_cc_proto",
        ":benchmark_descriptor_sv_cc_proto",
        ":benchmark_descriptor_upb_proto",
        ":benchmark_descriptor_upb_proto_reflection",
        "//:descriptor_upb_proto",
        "//:reflection",
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_protobuf//:protobuf",
    ],
)

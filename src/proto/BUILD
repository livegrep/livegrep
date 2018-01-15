package(default_visibility = ["//visibility:public"])

load("@org_pubref_rules_protobuf//go:rules.bzl", "go_proto_library")
load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")

go_proto_library(
    name = "go_proto",
    protos = ["livegrep.proto"],
    with_grpc = True,
)

cc_proto_library(
    name = "cc_proto",
    protos = ["livegrep.proto"],
    with_grpc = True,
)

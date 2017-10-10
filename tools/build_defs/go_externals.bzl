load(
    "@io_bazel_rules_go//go:def.bzl",
    "go_repository",
)

def _github(repo, commit):
  name = "com_github_" + repo.replace("/", "_").replace("-", "_").replace(".", "_")
  importpath = "github.com/" + repo
  return struct(
    name = name,
    importpath = importpath,
    commit = commit)

def _golang_x(pkg, commit):
  name = "org_golang_x_" + pkg
  importpath = "golang.org/x/" + pkg
  return struct(
    name = name,
    importpath = importpath,
    commit = commit)

_externals = [
    _golang_x("net", "d212a1ef2de2f5d441c327b8f26cf3ea3ea9f265"),
    _golang_x("text", "a9a820217f98f7c8a207ec1e45a874e1fe12c478"),
    _golang_x("oauth2", "a6bd8cefa1811bd24b86f8902872e4e8225f74c4"),
    struct(
        name = "org_golang_google_appengine",
        commit = "170382fa85b10b94728989dfcf6cc818b335c952",
        importpath = "google.golang.org/appengine/",
        remote = "https://github.com/golang/appengine",
        vcs = "git",
    ),
    _github("google/go-github", "e8d46665e050742f457a58088b1e6b794b2ae966"),
    _github("honeycombio/libhoney-go", "a8716c5861ae19c1e2baaad52dd59ba64b902bde"),
    _github("nelhage/go.cli", "2aeb96ef8025f3646befae8353b90f95e9e79bdc"),
    _github("bmizerany/pat", "c068ca2f0aacee5ac3681d68e4d0a003b7d1fd2c"),
    _github("google/go-querystring", "53e6ce116135b80d037921a7fdd5138cf32d7a8a"),
    _github("facebookgo/muster", "fd3d7953fd52354a74b9f6b3d70d0c9650c4ec2a"),
    _github("facebookgo/limitgroup", "6abd8d71ec01451d7f1929eacaa263bbe2935d05"),
    _github("facebookgo/clock", "600d898af40aa09a7a93ecb9265d87b0504b6f03"),
    struct(
        name = "in_gopkg_alexcesaro_statsd_v2",
        commit = "7fea3f0d2fab1ad973e641e51dba45443a311a90",
        importpath = "gopkg.in/alexcesaro/statsd.v2",
    ),
    struct(
        name = "in_gopkg_check_v1",
        commit = "20d25e2804050c1cd24a7eea1e7a6447dd0e74ec",
        importpath = "gopkg.in/check.v1",
    ),
]

def go_externals():
  for ext in _externals:
    if hasattr(ext, 'vcs'):
      go_repository(
        name = ext.name,
        importpath = ext.importpath,
        commit = ext.commit,
        vcs = ext.vcs,
        remote = ext.remote)
    else:
      go_repository(
        name = ext.name,
        importpath = ext.importpath,
        commit = ext.commit)

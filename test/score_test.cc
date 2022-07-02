#include <string.h>
#include "gtest/gtest.h"

#include "src/score.h"

TEST(ScoreTest, ScoresFilesAsExpected) {
    std::vector< std::pair<std::string, int>> testcases = {

        // machine generated
        {"web/htdocs/assets/3d/bootstrap.min.css", -100},
        {"web/htdocs/assets/3d/bootstrap.min.js", -100},
        {"web/htdocs/assets/3d/bootstrap.js.map", -100},
        {"web/htdocs/assets/bundle.js", -100},
        {"web/htdocs/assets/bundle.jsx", -100},
        {"src/_pb2/types.proto", -100},
        {"web/htdocs/assets/generated/cool-file.js", -100},
        {"web/htdocs/assets/cool-generated-file.js", -100},
        {"web/htdocs/assets/abcd.minified.js", -100},
        {"web/htdocs/assets/minified/README.txt", -100},
        {"src/minified_card.cpp", -100},
        {"tunnel.minified.php", -100},
        {"package-lock.json", -100},
        {"yarn.lock", -100},
        {"go.sum", -100},

        // vendored or third_party
        {"web/node_modules/folder/file.js", -100},
        {"web/node_modules/folder/folder/file2.js", -100},
        {"vendor/cloud.google.com/go/iam/iam.go", -100},
        {"github.com/cloud.google.com/go/iam/iam.go", -100},
        {"third_party/BUILD.divsufsort", -100},
        {"thirdparty/BUILD.divsufsort", -100},

        // test code
        {"client/test/BUILD", -50},
        {"client/test/suites/benchmarks", -50},
        {"__tests__/something.jsx", -50},
        {"src/some-test.jsx", -50},
        {"src/something.spec.jsx", -50},

        // vendored/third_party test code
        {"web/node_modules/__tests__/x.js", -150},

        // machine generated test code
        {"web/generated/test.js", -150},

        // files that should not be downranked
        // e.g. - not machine generated, not vendor/third_party code and not
        // tests
        {"README.md", 0},
        {"src/lib/fs.h", 0},
        {"src/codesearch.cc", 0},
        {"server/api/types.go", 0},
        {"src/proto/config.proto", 0},
        {"cmd/livegrep-fetch-reindex/main.go", 0},
        {"docker/base/Dockerfile", 0},
    };

    for (auto it = testcases.begin(); it != testcases.end(); ++it) {
        EXPECT_TRUE(it->second == score_file(it->first)) << "unexpected score for : " << it->first;
    }
}

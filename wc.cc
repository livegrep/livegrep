#include <stdio.h>

#include <git2.h>

int main(int argc, char **argv) {

    git_repository *repo;
    git_repository_open(&repo, ".");

    return 0;
}

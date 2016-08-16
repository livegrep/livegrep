def _new_libgit2_archive_impl(ctx):
  tgz = "libgit2-" + ctx.attr.version + ".tar.gz"
  ctx.download(
    ctx.attr.url,
    tgz,
    ctx.attr.sha256,
  )
  ctx.symlink(ctx.attr.build_file, "BUILD")
  cmd = ctx.execute([
    "tar", "-xzf", tgz,
    "--strip-components=1",
    "--exclude=libgit2-" + ctx.attr.version + "/tests"
  ])

  if cmd.return_code != 0:
    fail("error unpacking: " + cmd.stderr)

new_libgit2_archive = repository_rule(
  implementation=_new_libgit2_archive_impl,
  attrs = {
    "url": attr.string(mandatory = True),
    "sha256": attr.string(),
    "build_file": attr.label(mandatory = True),
    "version": attr.string(mandatory = True),
  })

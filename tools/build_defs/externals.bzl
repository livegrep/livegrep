def _new_patched_http_archive_impl(ctx):
  ctx.download_and_extract(
    ctx.attr.url,
    ctx.attr.add_prefix,
    ctx.attr.sha256,
    ctx.attr.type,
    ctx.attr.strip_prefix,
  )
  ctx.symlink(ctx.attr.build_file, "BUILD")
  cmd = ctx.execute(
    ["patch", "-d", ctx.attr.add_prefix, "-i", ctx.path(ctx.attr.patch_file)] +
    ctx.attr.patch_args,)
  if cmd.return_code != 0:
    fail("error patching: " + cmd.stderr)

new_patched_http_archive = repository_rule(
  implementation=_new_patched_http_archive_impl,
  attrs = {
    "url": attr.string(mandatory = True),
    "sha256": attr.string(),
    "build_file": attr.label(mandatory = True),
    "patch_file": attr.label(mandatory = True),
    "strip_prefix": attr.string(),
    "type": attr.string(),
    "patch_args": attr.string_list(default=["-p1"]),
    "add_prefix": attr.string(default="."),
  })

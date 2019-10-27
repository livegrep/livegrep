// Verifies that a module can be imported, and that it has a certain
// version. See `npm_library` for more details.

// Read --import_name= and --import_version= from args.
var importName;
var importVersion;
var noImportMainTest = false;

for (var i = 0; i < process.argv.length; i++) {
  var arg = process.argv[i];
  if (arg.startsWith('--import_name=')) {
    importName = arg.slice('--import_name='.length);
    continue;
  }
  if (arg.startsWith('--import_version=')) {
    importVersion = arg.slice('--import_version='.length);
    continue;
  }
  if (arg === '--no_import_main_test') {
    noImportMainTest = true;
    continue;
  }
}

if (!importName) {
  console.error("Argument `--import_name=` missing");
  process.exit(1);
}

if (!importVersion) {
  console.error("Argument `--import_version=` missing");
  process.exit(1);
}

// Check that the import is valid
try {
  if (!noImportMainTest) {
    require(importName);
  }
} catch (e) {
  console.error("Could not import module: " + importName);
  console.error(e);
  console.error(
    "If this module does not have a `main` js file, you should set the\n" +
      "attribute `no_import_main_test = True` to skip that check.\n" +
      "The default main is `index.js` in the root folder. Otherwise, the\n" +
      "main is set to the `main` key in the module's package.json.");
  process.exit(1);
}

// Read the import's version.
var packageJSON = require(importName + '/package.json');

if (packageJSON.version !== importVersion) {
  console.error("Import supplied version does not match version received: " +
              "Version wanted: " + importVersion + ", version received: " + packageJSON.version);
  process.exit(1);
}

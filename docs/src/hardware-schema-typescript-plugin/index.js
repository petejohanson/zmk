/* TODO: COPYRIGHT AND LICENSE HEADER */

var PrebuildPlugin = require("prebuild-webpack-plugin");
const fs = require("fs");
const { compileFromFile } = require('json-schema-to-typescript');

function generateHardwareMetadataTypescript() {
  compileFromFile("../schema/hardware-metadata.schema.json").then(ts => fs.writeFileSync("src/hardware-metadata.d.ts", ts)).catch(e => console.error(e));
}

module.exports = function () {
  return {
    name: "hardware-metadata-typescript-plugin",
    configureWebpack() {
      return {
        plugins: [
          new PrebuildPlugin({
            build: generateHardwareMetadataTypescript,
          }),
        ],
      };
    },
  };
};
/* TODO: COPYRIGHT AND LICENSE HEADER */

var PrebuildPlugin = require("prebuild-webpack-plugin");
const fs = require("fs");
const glob = require("glob");
const yaml = require("js-yaml");
const Mustache = require("mustache");

function generateSetupScripts() {
  return glob("../app/boards/**/*.zmk.yml", (error, files) => {
    const aggregated = files.flatMap((f) =>
      yaml.safeLoadAll(fs.readFileSync(f, "utf8"))
    );

    const data = aggregated.reduce(
      (agg, item) => {
        switch (item.type) {
          case "shield":
            item.compatible = true;
            item.split = item.parts?.length > 1;
            agg.keyboards.push(item);
            break;
          case "mcu":
            agg.boards.push(item);
            break;
        }
        return agg;
      },
      { keyboards: [], boards: [] }
    );

    const templateBuffer = fs.readFileSync(
      "src/templates/setup.sh.hbs",
      "utf8"
    );
    const script = Mustache.render(templateBuffer, data);
    fs.writeFileSync("static/setup.sh", script);
    const powershellTemplateBuffer = fs.readFileSync(
      "src/templates/setup.ps1.hbs",
      "utf8"
    );
    const psScript = Mustache.render(powershellTemplateBuffer, data);
    return fs.writeFileSync("static/setup.ps1", psScript);
  });
}

module.exports = function () {
  return {
    name: "setup-script-generation-plugin",
    configureWebpack() {
      return {
        plugins: [
          new PrebuildPlugin({
            build: generateSetupScripts,
          }),
        ],
      };
    },
  };
};

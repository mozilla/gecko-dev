/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const path = require("path");
const webpack = require("webpack");

module.exports = (env = {}) => ({
  mode: "none",
  devtool: env.development ? "inline-source-map" : false,
  plugins: [
    new webpack.BannerPlugin(
      `THIS FILE IS AUTO-GENERATED: ${path.basename(__filename)}`
    ),
    new webpack.optimize.ModuleConcatenationPlugin(),
  ],
  module: {
    rules: [
      {
        test: /\.jsx?$/,
        exclude: /node_modules\/(?!@fluent\/).*/,
        loader: "babel-loader",
        options: {
          presets: ["@babel/preset-react"],
        },
      },
    ],
  },
  resolve: {
    extensions: [".js", ".jsx", ".mjs"],
    modules: ["node_modules", "."],
  },
});

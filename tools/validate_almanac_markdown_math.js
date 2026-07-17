#!/usr/bin/env node
"use strict";

const fs = require("fs");
const katex = require("/usr/share/code/resources/app/node_modules/katex");

function maskFencedCode(markdown) {
  return markdown.replace(/```[\s\S]*?```/g, (block) => " ".repeat(block.length));
}

function mathExpressions(markdown) {
  let scan = maskFencedCode(markdown);
  const expressions = [];
  const displays = [];

  for (const match of scan.matchAll(/\$\$([\s\S]*?)\$\$/g)) {
    expressions.push({ position: match.index, math: match[1], display: true });
    displays.push([match.index, match.index + match[0].length]);
  }

  const characters = [...scan];
  for (const [start, end] of displays) {
    characters.fill(" ", start, end);
  }
  scan = characters.join("");

  for (const match of scan.matchAll(/(?<!\\)\$(?!\$)([^\n]*?)(?<!\\)\$/g)) {
    expressions.push({ position: match.index, math: match[1], display: false });
  }
  return expressions.sort((left, right) => left.position - right.position);
}

function sourcePage(markdown, position) {
  const prefix = markdown.slice(0, position);
  const matches = [...prefix.matchAll(/<!-- page-([^ ]+) -->/g)];
  return matches.length ? matches[matches.length - 1][1] : "front";
}

function validate(path) {
  const markdown = fs.readFileSync(path, "utf8");
  const expressions = mathExpressions(markdown);
  const errors = [];

  for (const expression of expressions) {
    try {
      katex.renderToString(expression.math, {
        displayMode: expression.display,
        strict: false,
        throwOnError: true,
      });
    } catch (error) {
      errors.push({
        ...expression,
        line: markdown.slice(0, expression.position).split("\n").length,
        page: sourcePage(markdown, expression.position),
        message: error.message,
      });
    }
  }

  console.log(`math expressions: ${expressions.length}`);
  console.log(`KaTeX errors: ${errors.length}`);
  for (const error of errors) {
    const formula = error.math.replace(/\s+/g, " ").trim().slice(0, 130);
    console.log(
      `page ${error.page}, line ${error.line}: ${error.message} :: ${formula}`,
    );
  }
  return errors.length;
}

if (process.argv.length !== 3) {
  console.error(`usage: ${process.argv[1]} MARKDOWN`);
  process.exit(2);
}
process.exitCode = validate(process.argv[2]) ? 1 : 0;

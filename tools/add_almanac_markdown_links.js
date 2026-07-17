#!/usr/bin/env node

const fs = require("fs");

const markdownPath = process.argv[2];
const referencePath = process.argv[3];

if (!markdownPath) {
  console.error(
    "Usage: node tools/add_almanac_markdown_links.js <book.md> [unlinked-reference.md]",
  );
  process.exit(1);
}

let lines = fs.readFileSync(markdownPath, "utf8").split("\n");
const bodyPages = new Set();
const frontPages = new Set();

for (const line of lines) {
  const bodyMatches = line.matchAll(/<!-- page-(\d{3}) -->/g);
  const front = line.match(/^<!-- _page-\d+-([ivxlcdm]+) -->$/i);

  for (const body of bodyMatches) bodyPages.add(body[1]);
  if (front) frontPages.add(front[1].toLowerCase());
}

function bodyTarget(page) {
  const number = Number(page);
  const padded = String(number).padStart(3, "0");
  return Number.isInteger(number) && bodyPages.has(padded)
    ? `#page-${padded}`
    : null;
}

function linkBodyPage(page) {
  const target = bodyTarget(page);
  return target ? `[${page}](${target})` : page;
}

let anchorsAdded = 0;
const anchored = [];

for (let index = 0; index < lines.length; index += 1) {
  const line = lines[index];
  const body = line.match(/^<!-- page-(\d{3}) -->$/);
  const front = line.match(/^<!-- _page-\d+-([ivxlcdm]+) -->$/i);
  let id = null;

  if (body) id = `page-${body[1]}`;
  if (front) id = `page-${front[1].toLowerCase()}`;

  if (!body && line.includes("<!-- page-")) {
    const inlineAnchored = line.replace(
      /<!-- page-(\d{3}) -->/g,
      (marker, page, offset, source) => {
        const anchor = `<a id="page-${page}"></a>`;
        if (source.slice(offset + marker.length).startsWith(anchor)) return marker;
        anchorsAdded += 1;
        return `${marker}${anchor}`;
      },
    );
    anchored.push(inlineAnchored);
    continue;
  }

  anchored.push(line);
  if (id && lines[index + 1] !== `<a id="${id}"></a>`) {
    anchored.push(`<a id="${id}"></a>`);
    anchorsAdded += 1;
  }
}

lines = anchored;

const contentsStart = lines.findIndex((line) => line === "## Contents");
const contentsEnd = lines.findIndex((line) => line === "<!-- _page-25-xxv -->");
let contentsLinksAdded = 0;
let referencePages = null;

function pageReference(line) {
  const roman = line.match(/([ivxlcdm]+)<br>$/i);
  if (roman && frontPages.has(roman[1].toLowerCase())) return roman[1];

  const arabic = line.match(/(\d+)<br>$/);
  if (!arabic) return null;

  if (/\s\d+<br>$/.test(line) && bodyTarget(arabic[1])) return arabic[1];
  for (let length = Math.min(3, arabic[1].length); length >= 1; length -= 1) {
    const candidate = arabic[1].slice(-length);
    if (bodyTarget(candidate)) return candidate;
  }
  return null;
}

if (referencePath) {
  const referenceLines = fs.readFileSync(referencePath, "utf8").split("\n");
  const start = referenceLines.findIndex((line) => line === "## Contents");
  const end = referenceLines.findIndex((line) => line === "<!-- _page-25-xxv -->");
  referencePages = referenceLines
    .slice(start, end)
    .filter((line) => line.endsWith("<br>"))
    .map(pageReference);
}

if (contentsStart !== -1 && contentsEnd !== -1) {
  let referenceIndex = 0;
  for (let index = contentsStart; index < contentsEnd; index += 1) {
    let line = lines[index].replace(
      /\[([0-9ivxlcdm]+)\]\(#page-[^)]+\)/gi,
      "$1",
    );
    if (!line.endsWith("<br>")) continue;

    const expectedPage = referencePages ? referencePages[referenceIndex] : null;
    referenceIndex += 1;

    if (expectedPage) {
      const core = line.slice(0, -4);
      if (/^[ivxlcdm]+$/i.test(expectedPage)) {
        const title = core.replace(/[ivxlcdm]+$/i, "").trimEnd();
        lines[index] = `${title} [${expectedPage}](#page-${expectedPage.toLowerCase()})<br>`;
        contentsLinksAdded += 1;
        continue;
      }

      const numericTail = core.match(/^(.*?)(\d[\d ]*)$/);
      if (numericTail) {
        const joined = numericTail[2].replace(/\s/g, "");
        if (joined.endsWith(expectedPage)) {
          const titleDigits = joined.slice(0, -expectedPage.length);
          let title = `${numericTail[1]}${titleDigits}`.trimEnd();
          if (title) title += " ";
          lines[index] = `${title}${linkBodyPage(expectedPage)}<br>`;
          contentsLinksAdded += 1;
          continue;
        }
      }
    }

    // Recover page numbers split by an earlier suffix-only pass (for example,
    // "Observer 2 1" is page 21, while "Order of 10 561" remains page 561).
    const splitPage = line.match(/^(.*?)(\d+)\s+(\d+)<br>$/);
    if (splitPage) {
      const combined = `${splitPage[2]}${splitPage[3]}`;
      if (combined.length <= 3 && bodyTarget(combined)) {
        line = `${splitPage[1]}${combined}<br>`;
      }
    }

    const roman = line.match(/^(.*?)([ivxlcdm]+)<br>$/i);
    if (roman && frontPages.has(roman[2].toLowerCase())) {
      const page = roman[2];
      lines[index] = `${roman[1]}[${page}](#page-${page.toLowerCase()})<br>`;
      contentsLinksAdded += 1;
      continue;
    }

    const arabic = line.match(/^(.*?)(\d+)<br>$/);
    if (!arabic) continue;

    const digits = arabic[2];
    let page = null;

    for (let length = Math.min(3, digits.length); length >= 1; length -= 1) {
      const candidate = digits.slice(-length);
      if (bodyTarget(candidate)) {
        page = candidate;
        break;
      }
    }

    if (!page) continue;

    const titleDigits = digits.slice(0, -page.length);
    let prefix = `${arabic[1]}${titleDigits}`;
    if (prefix && !/\s$/.test(prefix)) prefix += " ";
    lines[index] = `${prefix}${linkBodyPage(page)}<br>`;
    contentsLinksAdded += 1;
  }
}

const indexStart = lines.findIndex((line) => line === "<!-- page-741 -->");
const reference = String.raw`\d{1,3}(?:\s*[\u2013-]\s*\d{1,3})?`;
const referenceSuffix = new RegExp(
  String.raw`(^\s*|,\s+)(${reference}(?:\s*,\s*${reference})*\s*[,;.]?)$`,
);
let indexEndpointsLinked = 0;

if (indexStart !== -1) {
  for (let index = indexStart; index < lines.length; index += 1) {
    const line = lines[index].replace(/\[(\d{1,3})\]\(#page-\d{3}\)/g, "$1");
    const trimmed = line.trim();

    if (
      !trimmed ||
      /^\d{1,3}$/.test(line) ||
      line.startsWith("<!--") ||
      line.startsWith("<a id=")
    ) {
      continue;
    }

    const match = referenceSuffix.exec(line);
    if (!match) continue;

    const linked = match[2].replace(/\d{1,3}/g, (page) => {
      const replacement = linkBodyPage(page);
      if (replacement !== page) indexEndpointsLinked += 1;
      return replacement;
    });

    if (linked !== match[2]) {
      lines[index] = `${line.slice(0, match.index)}${match[1]}${linked}`;
    }
  }
}

const output = lines.join("\n");
const anchorIds = new Set(
  [...output.matchAll(/<a id="(page-[^"]+)"><\/a>/g)].map((match) => match[1]),
);
const targets = [...output.matchAll(/\]\(#(page-[^)]+)\)/g)].map(
  (match) => match[1],
);
const missingTargets = [...new Set(targets.filter((target) => !anchorIds.has(target)))];

if (missingTargets.length > 0) {
  throw new Error(`Missing page anchors: ${missingTargets.join(", ")}`);
}

fs.writeFileSync(markdownPath, output, "utf8");

console.log(`Added ${anchorsAdded} page anchors.`);
console.log(`Linked ${contentsLinksAdded} contents/list entries.`);
console.log(`Linked ${indexEndpointsLinked} index page-reference endpoints.`);
console.log(`Verified ${targets.length} internal page links.`);

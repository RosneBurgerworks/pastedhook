const fs = require("fs");
const path = require("path");

// Paths are relative to the repo root, wherever the script is run from
const root = path.join(__dirname, "..");

var fullClassTable = {};
try {
	fullClassTable = JSON.parse(fs.readFileSync(path.join(root, "full-class-table.json")).toString());
} catch (e) {}

console.log("Generating dummy class header");
console.log(fullClassTable.Stringify);
var header = `/*
	AUTO-GENERATED HEADER - DO NOT MODIFY
	NON-CONSTEXPR HEADER FOR $mod
*/

#ifndef DUMMY_AUTOGEN_HPP
#define DUMMY_AUTOGEN_HPP

namespace client_classes {
	
	class dummy {
	public:
`;

for (var clz in fullClassTable) {
	header += "\t\tint " + clz + " { 0 };\n";
}

header += `
	};
	
	extern dummy dummy_list;
}

#endif /* DUMMY_AUTOGEN_HPP */`;

console.log(header);
fs.writeFileSync(path.join(root, "include/classinfo/dummy.gen.hpp"), header);
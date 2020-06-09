const fs = require("fs");

var fullClassTable = {};
try {
	fullClassTable = JSON.parse(fs.readFileSync("full-class-table.json").toString());
} catch (e) {}

const file = fs.readFileSync(process.argv[2]).toString().split("\n");
const modname = process.argv[3];

console.log("Generating info for", modname, "from", process.argv[2]);

var classes = {};
for (var i in file) {
	var classInfo = /\[(\d+)\] (\w+)/gi.exec(file[i]);	
	if (classInfo) {
		fullClassTable[classInfo[2]] = true;
		classes[classInfo[2]] = parseInt(classInfo[1]);
	}
}

fs.writeFileSync("full-class-table.json", JSON.stringify(fullClassTable));

var headerConstexpr = `/*
	AUTO-GENERATED HEADER - DO NOT MODIFY
	CONSTEXPR HEADER FOR $mod
*/

#ifndef $mod_CONSTEXPR_AUTOGEN_HPP
#define $mod_CONSTEXPR_AUTOGEN_HPP

namespace client_classes_constexpr {
	
	class $mod {
	public:
`;

var header = `/*
	AUTO-GENERATED HEADER - DO NOT MODIFY
	NON-CONSTEXPR HEADER FOR $mod
*/

#ifndef $mod_AUTOGEN_HPP
#define $mod_AUTOGEN_HPP

namespace client_classes {
	
	class $mod {
	public:
`;

for (var clz in fullClassTable) {
	var value = "0";
	if (classes[clz]) value = classes[clz];
	headerConstexpr += "\t\tstatic constexpr int " + clz + " = " + value + ";\n";
	header += "\t\tint " + clz + " { " + value + " };\n";
}

header += `
	};
	
	extern $mod $mod_list;
}

#endif /* $mod_AUTOGEN_HPP */`;

headerConstexpr += `
	};
}

#endif /* $mod_CONSTEXPR_AUTOGEN_HPP */`;

fs.writeFileSync("include/classinfo/" + modname + ".gen.hpp", header.replace(/\$mod/g, modname));
fs.writeFileSync("include/classinfo/" + modname + "_constexpr.gen.hpp", headerConstexpr.replace(/\$mod/g, modname));


console.log(classes);

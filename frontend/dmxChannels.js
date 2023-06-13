const channelsPerAddress = 6;

const splitter = "---";

const normalParsed = [
  {
    name: "Bühne Links Außen Hinten",
    groups: [
      "Bühne",
      "Links",
      "Bühne Links",
      "Bühne Hinten",
      "Bühne Außen",
      "Bühne Links Außen",
      "Bühne Links Hinten",
    ],
  },
  {
    name: "Bühne Links Innen Hinten",
    groups: [
      "Bühne",
      "Links",
      "Bühne Links",
      "Bühne Hinten",
      "Bühne Innen",
      "Bühne Links Innen",
      "Bühne Links Hinten",
    ],
  },
  //
  {
    name: "Bühne Rechts Innen Hinten",
    groups: [
      "Bühne",
      "Rechts",
      "Bühne Rechts",
      "Bühne Hinten",
      "Bühne Innen",
      "Bühne Rechts Innen",
      "Bühne Rechts Hinten",
    ],
  },
  {
    name: "Bühne Rechts Außen Hinten",
    groups: [
      "Bühne",
      "Rechts",
      "Bühne Rechts",
      "Bühne Hinten",
      "Bühne Außen",
      "Bühne Rechts Außen",
      "Bühne Rechts Hinten",
    ],
  },
  //
  {
    name: "Bühne Links Außen Vorne",
    groups: [
      "Bühne",
      "Links",
      "Bühne Links",
      "Bühne Vorne",
      "Bühne Außen",
      "Bühne Links Außen",
      "Bühne Links Vorne",
    ],
  }, 
  {
    name: "Bühne Links Innen Vorne",
    groups: [
      "Bühne",
      "Links",
      "Bühne Links",
      "Bühne Vorne",
      "Bühne Innen",
      "Bühne Links Innen",
      "Bühne Links Vorne",
    ],
  },
  // 
  {
    name: "Bühne Rechts Innen Vorne",
    groups: [
      "Bühne",
      "Rechts",
      "Bühne Rechts",
      "Bühne Vorne",
      "Bühne Innen",
      "Bühne Rechts Innen",
      "Bühne Rechts Vorne",
    ],
  },
  {
    name: "Bühne Rechts Außen Vorne",
    groups: [
      "Bühne",
      "Rechts",
      "Bühne Rechts",
      "Bühne Vorne",
      "Bühne Außen",
      "Bühne Rechts Außen",
      "Bühne Rechts Vorne",
    ],
  },
];

const ledStart = (normalParsed.length * channelsPerAddress);

const LEDsParsed = [
  {
    name: "Spot Links Oben",
    groups: ["Spots", "Spots Links", "Spots Oben", "Links"],
  },
  {
    name: "Spot Links Unten",
    groups: ["Spots", "Spots Links", "Spots Unten", "Links"],
  },
  {
    name: "Spot Rechts Oben",
    groups: ["Spots", "Spots Rechts", "Spots Oben", "Rechts"],
  },
  {
    name: "Spot Rechts Unten",
    groups: ["Spots", "Spots Rechts", "Spots Unten", "Rechts"],
  },
];

let str = normalParsed.map((light, idx) => {
  const startChan = idx * channelsPerAddress + 1;
  console.log(
    [light.name, startChan, startChan + channelsPerAddress - 1].join(" \t ")
  );
  return [light.name, startChan, 1, light.groups.join(",")].join(";");
});

let strLED = LEDsParsed.map((light, idx) => {
  const startChan = idx * channelsPerAddress + 1 + ledStart;
  console.log(
    [light.name, startChan, startChan + channelsPerAddress - 1].join(" \t ")
  );
  return [light.name, startChan, 6, light.groups.join(",")].join(";");
});

console.log([...str, ...strLED].join(splitter));

import React from "react";

import { HardwareMetadata } from "../hardware-metadata";

interface HardwareListProps {
  items: HardwareMetadata[];
}

function HardwareLineItem({ item }: { item: HardwareMetadata }) {
  return (
    <li>
      <a href={item.url}>{item.name}</a> (<code>{item.id}</code>)
    </li>
  );
}

function HardwareList({ items }: HardwareListProps) {
  let grouped: {
    [index in "mcu" | "keyboard" | "shield"]?: HardwareMetadata[];
  } = items.reduce((agg, hm) => {
    agg[hm.type] = [...(agg[hm.type] || []), hm];
    return agg;
  }, {});

  console.debug(grouped);
  return (
    <>
      <h2>Boards (Controllers + Integrated Keyboards</h2>
      <ul>
        {grouped["mcu"]
          ?.sort((a, b) => a.name.localeCompare(b.name))
          ?.map((s) => (
            <HardwareLineItem item={s} />
          ))}
        {grouped["keyboard"]
          ?.sort((a, b) => a.name.localeCompare(b.name))
          ?.map((s) => (
            <HardwareLineItem item={s} />
          ))}
      </ul>
      <h2>Keyboard Shields</h2>
      <ul>
        {grouped["shield"]
          ?.sort((a, b) => a.name.localeCompare(b.name))
          ?.map((s) => (
            <HardwareLineItem item={s} />
          ))}
      </ul>
    </>
  );
}

export default HardwareList;

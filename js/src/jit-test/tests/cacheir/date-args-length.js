var args = new Array(1000).fill(0).join(",");

eval(`new Date().getTime(${args})`);
eval(`new Date().valueOf(${args})`);
eval(`new Date().getFullYear(${args})`);
eval(`new Date().getMonth(${args})`);
eval(`new Date().getDate(${args})`);
eval(`new Date().getDay(${args})`);
eval(`new Date().getHours(${args})`);
eval(`new Date().getMinutes(${args})`);
eval(`new Date().getSeconds(${args})`);

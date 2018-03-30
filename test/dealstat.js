const fs = require('fs');

const file = fs.readFileSync('logfile');

const data = file.toString('utf8').split('\n');

let rt = 0;
let ow = 0;
let len = data.length;
let c = 0;
console.log(len);

for (let e of data) {
	c++;
	const parts = e.split(',');
	const [route, type, count] = parts;
	if (type == 'OW') {
		ow++;
	} else {
		rt++;
	}
	if (c %100000 ==0){
		console.log(c, ow, rt);
	}
}
console.log(c, ow, rt);

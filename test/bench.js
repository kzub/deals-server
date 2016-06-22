var http = require('http');
var async = require('async');

var response_body_example = '43;120;120;120;120;120;121;121;121;120;121;{"origin":"MOW","departure_date":"2016-08-23","destination":"MAD","return_date":"2016-03-19","direct":true,"price":7133}{"origin":"MOW","departure_date":"2016-03-19","destination":"LON","return_date":"2016-07-02","direct":true,"price":8389}{"origin":"MOW","departure_date":"2016-09-09","destination":"JFK","return_date":"2016-08-22","direct":true,"price":8659}{"origin":"MOW","departure_date":"2016-06-12","destination":"PAR","return_date":"2016-07-27","direct":true,"price":9739}{"origin":"MOW","departure_date":"2016-07-13","destination":"AER","return_date":"2016-10-19","direct":true,"price":7205}{"origin":"MOW","departure_date":"2016-05-10","destination":"FRA","return_date":"2016-11-15","direct":true,"price":11042}{"origin":"MOW","departure_date":"2016-09-14","destination":"LAX","return_date":"2016-08-09","direct":false,"price":8334}{"origin":"MOW","departure_date":"2016-03-19","destination":"OVB","return_date":"2016-11-14","direct":false,"price":7001}{"origin":"MOW","departure_date":"2016-09-26","destination":"BAR","return_date":"2016-07-08","direct":true,"price":6871}{"origin":"MOW","departure_date":"2016-03-09","destination":"BER","return_date":"2016-06-20","direct":true,"price":10290}';
var cities = ['MOW', 'MAD', 'BER', 'BAR', 'FRA', 'PAR', 'AER', 'OVB', 'LON', 'JFK', 'LAX', 'XAZ','YZS','TEK','EUX','URJ','ROT','HUU','JST','HGH','QWF','TRD','YXJ','YGV','GNB','LDY','YGH','YZR','LTQ','KIX','FUN','LIT','XDM','PHL','INC','URT','DJB','SVB','ATQ','DLM','RKT','DBA','WBQ','DAT','MAM','NOC','BKG','CIH','NAH','UTT','AGA','EAT','KYP','SVI','IKT','TRV','QQH','TKV','YZZ','HGD','WJU','ZYW','HBZ','CME','PLJ','ATM','TIJ','JIM','YRF','LUO','XVC','LBB','CVM','YHZ','XFD','PZU','YHB','AEX','SYZ','ABE','OSU','AEG','DYU','QXB','ASB','HLN','DAC','XWS','MCW','BRI','LKH','XPT','YGK','NSI'];


function parseAnswer(response_body){
	var pos = 0;
	var info_length;

	while(pos < 100){
		if(response_body[pos++] === ';') {
			info_length = Number(response_body.slice(0, pos - 1));
			break;
		}
	}

	if(!isFinite(info_length)){
		console.log('error 343');
		return;
	}

	var sizes = response_body.slice(0, info_length - 1).split(';');
	var data = [];
	var pointer = Number(sizes[0]);

	for(var i = 1; i < sizes.length; i++){
		var size = Number(sizes[i]);
		var textdata = response_body.slice(pointer, pointer + size);
		pointer += size;
		try {
			data.push(JSON.parse(textdata));
		}catch(e){
			console.log('catch', e);
			console.log(response_body);
		}
	}

	if(!data){
		console.log(data);		
	}

	return data;	
}

var server_ports = [5000/*, 5001, 5002, 5003*/];
function getRandomPort(){
	return server_ports[Math.round(Math.random()*(server_ports.length-1))];
}

function httpsend(mode, params, postData, callback){
	var modes = {
		'add': {
			path: '/deals/add',
			method: 'POST'
		},
		'top': {
			path: '/deals/top',
			method: 'GET'
		}
	}

	if(!modes[mode]){
		callback('bad mode');
		return;
	}

	var qs = '?';

	for(key in params){
		qs += key + '=' + params[key] + '&';
	}

	var options = {
	  hostname: '127.0.0.1',
	  // hostname: '192.168.55.100',
	  // port: 5000,
	  port: getRandomPort(),
	  path: modes[mode].path + qs,
	  method: modes[mode].method,
	  headers: {
	    'Content-Type': 'text/plain'
	  }
	};

	if(modes[mode].method === 'POST'){
		options.headers['Content-Length'] = postData.length;
	}
	// console.log(options.path)
	var data = '';
	var req = http.request(options, function(res) {
	  if(res.statusCode !== 200) {
	  	console.log('STATUS:' + res.statusCode);
	  }
	  // console.log(`HEADERS: ${JSON.stringify(res.headers)}`);
	  res.setEncoding('utf8');
	  res.on('data', function(chunk) {
	    data += chunk;
	  });
	  res.on('end', function() {
	  	// console.log(data);
	    callback(undefined, data);
	  })
	});

	req.on('error', function(e) {
	  console.log('problem with request:', e.message);
		callback('request error');
	});

	// write data to request body
	if(modes[mode].method === 'POST'){
		req.write(postData);
	}
	req.end();
}

function getRandomCityPair(){
	var key1 = Math.floor(Math.random() * cities.length);
	var key2;
	do {
		key2 = Math.floor(Math.random() * cities.length);
	} while (key2 == key1);

	// console.log(cities[key1], cities[key2]);
	return [cities[key1], cities[key2]];
}

function getRandomDate(){
	var year = 2016;
	var month = 1 + Math.floor(Math.random()*11);
	var day = 1 + Math.floor(Math.random()*27);

	if (month < 10) { month = '0' + month; }
	if (day < 10) { day = '0' + day; }
	
	return [year, month, day].join('-');
}

function getRandomPrice(){
	var price = 5000 + Math.floor(Math.random()*15000) + Math.floor(Math.random()*15000);
	return price;
}


var testcount = process.argv[2];
var skipget = false;

if(!isFinite(testcount)){
	console.log('node bench.js <testcount>\n');
	return;
}
if(process.argv[3] == "noget"){
	skipget = true;
}

var timer1 = Date.now();
var counter1 = testcount;
var timer2 = Date.now();
var counter2 = 0;


var stop = false;
if(process.argv[3] != "noset"){
async.until(
		function test(){
			testcount--;
			if(testcount % 500 === 0){
				var timeframe = Date.now() - timer1;
				var ticks = counter1 - testcount;
				var rate = 1000 * ticks / timeframe;
				timer1 = Date.now();
				counter1 = testcount;
				console.log('ADD rate:' + rate.toFixed(2));
			}
			return !testcount;
		},
		function iter(cb) {
				var pair = getRandomCityPair();
				var params = {
					origin: pair[0],
					departure_date: getRandomDate(), 
					destination: pair[1],
					return_date: getRandomDate(),
					direct: Math.random() > 0.5,
					price: getRandomPrice()
				};
				httpsend('add', params, JSON.stringify(params), cb);
		},
		function done(err, data) {
			stop = true;
			console.log('add done');
		}
	);
}

if(skipget){
	return;
}

async.until(
	function test() {
		if(counter2++ % 500 === 0){
			var timeframe = Date.now() - timer2;
			var ticks = 500;
			var rate = 1000 * ticks / timeframe;
			timer2 = Date.now();
			console.log('TOP rate:' + rate.toFixed(2));
		}
		return stop;
	},
	function iter(cb) {
		httpsend('top', { origin: getRandomCityPair()[0]}, undefined, function(err, data){
			if(!err){
				parseAnswer(data);
			}
			cb();
		});
	},
	function done() {
		console.log('top done')
	}
);




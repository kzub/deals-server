var async = require('async');
var Step = require('step');
var zlib = require('zlib');


function parseServerResponse(response_body, callback){
	var pos = 0;
	var info_length;

	// response format
	// <-  size_info  -><-     data blocks       ->
	// ↓ size_info block length
	// 11;121;121;45;21;{....},{....},{....},{....}
	//    ↑   ↑   ↑  ↑  each data block length

	while(pos < 100){
		if(response_body[pos++] === 0x3b) {
			info_length = Number(response_body.slice(0, pos - 1));
			break;
		}
	}

	if(!isFinite(info_length)){
		console.error('parseServerResponse_1', response_body.slice(0, 200).toString());
		callback('ERROR_DEALS_RESPONSE_FORMAT');
		return;
	}

	var sizes = response_body.slice(0, info_length - 1).toString().split(';');
	var data = [];
	var pointer = Number(sizes[0]);

	if(!isFinite(pointer)){
		callback('ERROR_DEALS_RESPONSE_SIZE_FORMAT');
		return;
	}

	for(var i = 1; i < sizes.length; i++){
		var size = Number(sizes[i]);
		if(!isFinite(size)){
			callback('ERROR_DEALS_RESPONSE_SIZE_FORMAT2');
			return;
		}
		var textdata = response_body.slice(pointer, pointer + size);
		pointer += size;

		data.push(textdata);
	}

	callback(undefined, data);
};

var usage = setTimeout(function function_name(argument) {
	console.log('USAGE   curl deals:8090/deals/top?origin=MOW | node check.js');
	process.exit();
}, 2000);

var data = new Buffer(0);
process.stdin.on('data', function(chunk){
	if(usage){
		clearTimeout(usage);
		usage = 0;		
	}
	data = Buffer.concat([data, chunk]);
});

process.stdin.on('end', function(){
	parseServerResponse(data, function(err, results) {
  		async.mapSeries(
				results,
				function iter(item, cb) {
					try {
						zlib.inflate(item, function(err, jsontext){
							var obj;
							try{
								obj = JSON.parse(jsontext);
							}catch(e){
								console.error('getDealsTop.parse2', 'length:', results.length,
									    'jsontext:', jsontext);
							}
							cb(undefined, obj);
						});
					}
					catch(e) {
						console.error('getDealsTop.unzip', e);
						cb();
					}
				},
				function(err, data){
					data = data.filter(function(o){ return o instanceof Object; });
					dataProcessor(data);
				}
			);
	});
});


function dataProcessor(datas) {
	for(var id in datas){
		var deal = datas[id];
		var segments = [];
		for(var id2 in deal.trips){
			var trip = deal.trips[id2];
			segments.push(trip.from + trip.to + '(' + trip.startDate + ')' + (id2==deal.trips.length-1?'':(trip.continued?'-':'===')));
			// console.log(trip)
		}
		console.log(deal.origin + '-' + deal.destination + ':' + deal.destination_country, deal.price, segments.join(''), new Date(deal.dateCreated).toJSON().slice(0, 19));
	}
}
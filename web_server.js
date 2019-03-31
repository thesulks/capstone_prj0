var express = require('express');
var app = express();
fs = require('fs');

mysql = require('mysql');
var account = require('./account_info.js');
var connection = mysql.createConnection(account.info);

connection.connect();

function insert_sensor(device, unit, type, value, seq, ip) {
    obj = {};
    obj.seq = seq;
    obj.device = device;
    obj.unit = unit;
    obj.type = type;
    obj.value = value;
    obj.ip = ip.replace(/^.*:/, '')

        var query = connection.query('insert into sensors set ?', obj, function(err, rows, cols) {
            if (err) throw err;
            console.log("database insertion ok= %j", obj);
        });
}

app.get('/', function(req, res) {
    res.end('Nice to meet you');
});

app.get('/log', function(req, res) {
    r = req.query;
    console.log("GET %j", r);

    insert_sensor(r.device, r.unit, r.type, r.value, r.seq, req.connection.remoteAddress);
    res.end('OK:' + JSON.stringify(req.query));
});

app.get('/graph', function (req, res) {
    console.log('got app.get(graph)');
    var html = fs.readFile('./graph.html', function (err, html) {
    html = " "+ html
    console.log('read file');

    var qstr = 'select * from sensors ';
    connection.query(qstr, function(err, rows, cols) {
        if (err) throw err;

        var data = "";
        var comma = "";
        var options = { year: 'numeric', month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit' };
        var t_start = rows[0].time.toLocaleString('en-US', options);
        var t_end = rows[rows.length-1].time.toLocaleString('en-US', options);
        
        for (var i = 0; i< rows.length; i++) {
            r = rows[i];
            date_string = String(r.time.getYear()+1900) + "," + String(r.time.getMonth()) + "," + String(r.time.getDate()) + ","
                        + String(r.time.getHours()) + "," + String(r.time.getMinutes());
            data += comma + "[new Date(" + date_string + "),"+ r.value +"]";
            comma = ",";
        }

        var header = "data.addColumn('date', 'Date/Time (START: " + t_start + " - END: " + t_end +  ")');"
        header += "data.addColumn('number', 'Temp');"
        html = html.replace("<%HEADER%>", header);
        html = html.replace("<%DATA%>", data);

        res.writeHeader(200, {"Content-Type": "text/html"});
        res.write(html);
        res.end();
        });
    });
})

var server = app.listen(9000, function () {
    var host = server.address().address
    var port = server.address().port
    console.log('listening at http://%s:%s', host, port)
});

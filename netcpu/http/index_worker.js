/*

Written and contributed by Leonid Zadorin at the Centre for the Study of Choice
(CenSoC), the University of Technology Sydney (UTS).

Copyright (c) 2012 The University of Technology Sydney (UTS), Australia
<www.uts.edu.au>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. All products which use or are derived from this software must display the
following acknowledgement:
  This product includes software developed by the Centre for the Study of
  Choice (CenSoC) at The University of Technology Sydney (UTS) and its
  contributors.
4. Neither the name of The University of Technology Sydney (UTS) nor the names
of the Centre for the Study of Choice (CenSoC) and contributors may be used to
endorse or promote products which use or are derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) AT THE
UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) AND CONTRIBUTORS ``AS IS'' AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) OR
THE UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

*/

importScripts("index_utils.js");   

function parse_csv(file, newrow_callback)
{
	var row = [];
	var str = [];
	var newrow_pending = true;
	var unmatched_quotes = false;
	var str_i = 0;

	var file_reader = new FileReaderSync();
	var offset_i;
	var new_offset_i = 0;
	
	function test_more_data()
	{
		if (str_i == str.length) {

			offset_i = new_offset_i;

			var file_slice = file.slice(offset_i, offset_i + 1000000); 
			str = file_reader.readAsBinaryString(file_slice);

			if (!str.length) {
				if (row.length > 0 && row[0].length > 0)
					newrow_callback(row, offset_i + str_i);
				return false;
			}

			str_i = 0;
			new_offset_i = offset_i + str.length;

		} 
		return true;
	}

	while (true) {
		if (test_more_data() == false)
			return;
		var c = str[str_i++];
		//if (/\s/.test(c) == false) {
		if (/[\041-\0176]/.test(c) == true) {
			if (newrow_pending == true) {
				newrow_pending = false;
				row.push("");
				while (true) {
					if (c == ',') {
						if (unmatched_quotes == false)
							row.push("");
						else 
							row[row.length - 1] += c;
					} else if (/[\r\n]/.test(c) == true) {
						if (newrow_callback(row, offset_i + str_i) == false)
							return;
						newrow_pending = true;
						row = [];
						break;
					} else if (c == '"') {
						if (unmatched_quotes == true) {
							if (test_more_data() == false)
								return;
							if (str[str_i] == '"') {
								row[row.length - 1] += c;
								++str_i;
							} else
								unmatched_quotes = false;
						} else
							unmatched_quotes = true;
					} else if (/\s/.test(c) == false || unmatched_quotes == true)
						row[row.length - 1] += c;
					if (test_more_data() == false)
						return;
					c = str[str_i++];
				}
			}
		}
	}
}

function preview_csv(file, rows) 
{
	//var file_slice = file.slice(0, Math.min(file.size, 30000));
	//var file_reader = new FileReader();
	//var file_reader = new FileReaderSync();
	//file_reader.onloadend = function() {
	//	if (this.readyState == FileReader.DONE) {
	//		split_csv(this.result);
	//		postMessage({"message": "preview_csv", "csv_grid": csv_grid});
	//	}
	//};
	//file_reader.readAsText(file_slice);
	//file_reader.readAsText(file);

	var rv = {};
	rv.message = "preview_csv";
	rv.csv_grid = [];

	parse_csv(file, 
		function (row, file_offset) 
		{
			if (!(rv.csv_grid.length % 5))
				postMessage({"message": "progress", "percent": (100 * rv.csv_grid.length) / rows});

			rv.csv_grid.push(row);
			if (rv.csv_grid.length > rows)
				return false;
			else 
				return true;
		}
	);

	postMessage(rv, [rv]);
}

function strip_csv(msg) 
{
	var rv = {};
	rv.message = "strip_csv";
	rv.csv = "";
	var row_count = -1;

	parse_csv(msg.file, 
		function (row, file_offset) 
		{
			if (!(++row_count % 500))
				postMessage({"message": "progress", "percent": (100 * file_offset) / msg.file.size});

			if (row_count + 1 < msg.fromrow)
				return true;

			var allow_row = true;
			for (var i = 0; allow_row == true && i != msg.delby.length; ++i) {
				var delby_i = msg.delby[i];
				var delby_value = parseInt(delby_i.val);
				function test_cell(x)
				{
					var cell_value = parseInt(row[x]);
					if (delby_i.op == "eq" && cell_value == delby_value)
						allow_row = false;
					else if (delby_i.op == "lt" && cell_value < delby_value)
						allow_row = false;
					else if (delby_i.op == "gt" && cell_value > delby_value)
						allow_row = false;
				}
				if (delby_i.column == '*') {
					for (var j = 0; j != msg.attributes.length; ++j)
						test_cell(row[msg.attributes[j] - 1]);
				} else {
					var column_i;
					if (/[A-Za-z]/.test(delby_i.column) == true)
						column_i = parseInt(column_alpha_to_numeric(delby_i.column));
					else
						column_i = parseInt(delby_i.column) - 1;
					test_cell(column_i);
				}
			}

			if (allow_row == true) {
				rv.csv += row[msg.respondent_i - 1] + ',' + row[msg.choice_set_i - 1] + ',' + row[msg.alternative_i - 1] + ',' + row[msg.choice_i - 1];
				for (var j = 0; j != msg.attributes.length; ++j)
					rv.csv += ',' + row[msg.attributes[j] - 1];
				rv.csv += "\r\n";
			}

			return true;
		}
	);

	postMessage(rv, [rv]);
}


onmessage = function(e) {
	if (e.data.message == "preview_csv") {
		preview_csv(e.data.file, e.data.rows);
	} else if (e.data.message == "strip_csv") {
		strip_csv(e.data);
	}
}


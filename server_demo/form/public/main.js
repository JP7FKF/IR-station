function getWifiList(){
	$.ajax({
		type:"GET",
		url:"/wifi-list",
		dataType:"json",
		cache:false,
		timeout:10000
	}).done(function(data) {
		$('#wifi-list').empty();
		$.each(data,function(index,wifi){
			$('#wifi-list').append($('<option>').val(wifi).text(wifi));
		});
		$('#wifi-list').append($('<option>').val("stealth-ssid").text("- use a stealth SSID -"));
		$('#info-status').text("Loading successful. Select a mode.")
		$('#form').show();
		$('#ap').show();
	}).fail(function(){
		$('#info-status').text('Connection Failed. Please Reload.');
	});
}
function form(){
	if(confirm("Are you sure to confirm?")){
		$('#form').hide();
		$('#ap').hide();
		$('#info-status').text("Connecting... Please wait...");
		$.get('/confirm',{
			ssid: ($('#form [name="ssid"]').val()=="stealth-ssid")?$('#form [name="stealth-ssid"]').val():$('#form [name="ssid"]').val(),
			stealth: ($('#form [name="ssid"]').val()=="stealth-ssid"),
			password: $('#form input[name="password"]').val(),
			hostname: $('#form input[name="hostname"]').val()
		}).fail(function(){
			$('#info-status').text('Connection Failed. Please Reload.');
		});
		var cnt = 0;
		timerID = setInterval(function(){
			cnt++;
			if(cnt > 20){
				$('#form').show();
				$('#ap').show();
				$('#info-status').text("Connection failed. Please try again.");
				clearInterval(timerID);
				timerID = null;
			}
			$.get('/isConnected').done(function(res){
				var url = $('#form input[name="url"]').val();
				if(res!="false"){
					clearInterval(timerID);
					timerID = null;
					$('#info-status').html(
						'Connection Successful.'
						+'<br/>'+
							'Please make a note of these URL.'
						+'<br/>'+
							'Screenshot is also good!'
						+'<br/>'+
							'For Apple device: <a href="http://'+url+'.local/">http://'+url+'.local/</a>'
						+'<br/>'+
							'For all device: <a href="http://'+res+'/">http://'+res+'/</a>'
					);
					$('#info button[name="reload"]').hide();
					$('#info button[name="reboot"]').show();
				}
			}).fail(function(){
				$('#info-status').text('Connection Failed. Please Reload.');
			});
		}, 1000);
	}
}
function setAP(){
	if(confirm("Can I setup as Access Point Mode?")){
		$('#form').toggle();
		$('#ap').toggle();
		$('#info-status').text("Connecting... Please wait.");
		$.get('/set-ap-mode',{
			url: $('#ap input[name="url"]').val()
		}).done(function(res){
			$('#info-status').text(res);
		}).fail(function(){
			$('#info-status').text('Connection Failed. Please Reload.');
		});
	}
}
$('#info button[name="reboot"]').click(function(){
	$.get('/reboot');
});

$('#form button').click(form);
$('#form input').keypress(function(e){
	if(e.which == 13){
		form();
	}
});

$('#ap button').click(setAP);
$('#ap input').keypress(function(e){
	if(e.which == 13){
		setAP();
	}
});

$('#wifi-list').change(function(){
	if($('#wifi-list option:selected').val()=="stealth-ssid"){
		$('#stealth-ssid-form').show();
	}else{
		$('#stealth-ssid-form').hide();
	}
});

getWifiList();


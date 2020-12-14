function display(expr, div)
{
	if (expr != "none")
		document.getElementById(div).style.visibility = "visible";
	else
		document.getElementById(div).style.visibility = "hidden";
	try 
	{
		document.getElementById(div).style.display = expr;
	}
	catch (e)
	{
		document.getElementById(div).style.display = "block";
	}
}

var getUrlParameter = function getUrlParameter(sParam)
{
	var sPageURL = decodeURIComponent(window.location.search.substring(1)),
		sURLVariables = sPageURL.split('&'),
		sParameterName,
		i;

	for (i = 0; i < sURLVariables.length; i++) {
		sParameterName = sURLVariables[i].split('=');

		if (sParameterName[0] === sParam) {
			return sParameterName[1] === undefined ? true : sParameterName[1];
		}
	}
};

function makeRequest(url, content, handler) {
	$.ajax({
		url: url, /*"/cgi-bin/makeRequest",*/
		type: 'POST',
		data: content,
		cache: false, 
		error: function(xhr) {

		},
		success: function(response) {
			if (handler)
				handler(response);
		}  
	});
}

function check_suport_lang(lang)
{
	var lan = lang.substring(0, 2);
	if (lan == 'en' ||
	    lan == 'zh' ||
	    lan == 'ja' ||
	    lan == 'ko' ||
	    lan == 'de' ||
	    lan == 'fr' ||
	    lan == 'nl' ||
	    lan == 'it' ||
	    lan == 'pt' ||
	    lan == 'es' ||
	    lan == 'sv' ||
	    lan == 'ar')
		return lang;
	else 
		return 'en-us';
}

function renameLang(lang) {
	lang = check_suport_lang(lang);
	lang = lang.replace(/_/, '-');

	if (lang.length > 3) {
		var lan = lang.substring(0, 3);
		var reg = lang.substring(3);

		if (lan == 'en-') {
			lang = lan+'us';
		} else if (lan == 'zh-') {
			if (reg != 'cn')
				lang = lan+'tw';
			else
				lang = lan+'cn';
		} else if (lan == 'de-') {
			lang = lan+'de';
		} else if (lan == 'fr-') {
			lang = lan+'fr';
		} else if (lan == 'nl-') {
			lang = lan+'nl';
		} else if (lan == 'it-') {
			lang = lan+'it';
		} else if (lan == 'pt-') {
			lang = lan+'pt';
		} else if (lan == 'es-') {
			lang = lan+'es';
		} else if (lan == 'sv-') {
			lang = lan+'se';
		} else if (lan == 'ar-') {
			lang = lan+'ar';
		}
	} else {
		if (lang == 'ja') {
			lang = 'ja-jp';
		} else if (lang == 'ko') {
			lang = 'ko-ko';
		} else if (lang == 'de') {
			lang = 'de-de';
		} else if (lang == 'fr') {
			lang = 'fr-fr';
		} else if (lang == 'nl') {
			lang = 'nl-nl';
		} else if (lang == 'it') {
			lang = 'it-it';
		} else if (lang == 'pt') {
			lang = 'pt-pt';
		} else if (lang == 'es') {
			lang = 'es-es';
		} else if (lang == 'sv') {
			lang = 'sv-se';
		} else if (lang == 'zh') {
			lang = 'zh-tw';
		} else if (lang == 'ar') {
			lang = 'ar-ar';
		} else {
			/*  if we get a wong lang-region string, return 'en-us'  */
			lang = 'en-us';
		}
	}

	return lang;
}

function InitLanguage(reload_text)
{
	var ret = true;
	var lang1 = window.navigator.userLanguage;
	var lang2 = window.navigator.language;

	if (lang1 != undefined) lang1 = lang1.toLowerCase();
	if (lang2 != undefined) lang2 = lang2.toLowerCase();

	var browser_lang = lang1 || lang2;
	if($('[name="lang"]').val() == "") {
		browser_lang = renameLang(browser_lang);
		$('[name="lang"]').val(browser_lang);
		ret = false;
	}
	var newLang = $('[name="lang"]').val();

	$.localise('/js/lang/lang', {language:newLang, loadBase:false, timeout:30000, async:true, complete: function(pkg){if(reload_text)reload_text();}});
	return ret;
}

function validateIPAddress(inputID, mask, empty_check, onSuccess, onIncomplete)
{
	/* trigger the jquery focusout event */
	$(inputID).on("focusout", function() {
		var success = 0;
		/*
		save the value of $(this) in the $this variable
		rather than writing $(this) everywhere
		*/
		var $this = $(this);
		/* save the entered string from input */
		var entered_ip = $this.val();

		/* check to see if that something was introduced into the field */
		if (entered_ip) {
			/*
			split the resulted string by "." character
			through the jquery split method and
			save the splited values into an array
			*/
			var ip_fields_array = entered_ip.split(".");

			/* make a html block for incorrect ip address error */
			var field_error = document.createElement("span");
			field_error.setAttribute("id", "field_error");
			var error_message;

			/* store the length of the array */
			var array_length = ip_fields_array.length;
			/*regular expression that starts with 0 and continue with any digit */
			var regex = /^0([0-9])+/;
			/* make an iteration through every item from array */
			for (var i = 0; i < array_length; i++) {
				var ip_field = ip_fields_array[i];
				
				ip_fields_array[i] = ip_fields_array[i].replace(/ /g, "");
				if(ip_fields_array[i].length > 1) {
					ip_field = ip_fields_array[i] = ip_fields_array[i].replace(/^0/g, "");
				}
				if(ip_fields_array[i].length > 1) {
					ip_field = ip_fields_array[i] = ip_fields_array[i].replace(/^0/g, "");
				}
				/*
				remove the " " char and then test the regex created
				true -> if we encounter a string that starts with 0
				and continue with any character (other than white space)
				false -> otherwise
				*/
				var regex_result = regex.test(ip_field.replace(/ /g, ""));
				/*
				show the error message if one ip field is entered ("   ")
				and if the regex created is true
				*/
				if (ip_field == "   " || regex_result == true) {
					if ($this.siblings("#field_error").length == 0) {
						/* append the html block with proper error */
						if(mask == 0)
							error_message = document.createTextNode(__incorrect_ip);
						else
							error_message = document.createTextNode(__incorrect_mask);
						field_error.appendChild(error_message);
						$this
							.parent()
							.append(field_error);
						$this
							.css({"border-color":"#d43f3a"})
							.addClass("has-error");
						if (onIncomplete) {
							onIncomplete(this);
						}
					}
					/* stop */
					return;
				} else {
					if(i == 0) {
						if(ip_field.replace(/ /g, "") == "0") {
							if ($this.siblings("#field_error").length == 0) {
								error_message = document.createTextNode(__incorrect_first_ip);
								field_error.appendChild(error_message);
								$this
									.parent()
									.append(field_error);
								$this
									.css({"border-color":"#d43f3a"})
									.addClass("has-error");
									if (onIncomplete) {
										onIncomplete(this);
									}
							}
							/* stop */
							return;
						}
					}
					if(mask == 0) {
						if(i == 3) {
							if(ip_field.replace(/ /g, "") == "0") {
								if ($this.siblings("#field_error").length == 0) {
									error_message = document.createTextNode(__incorrect_last_ip);
									field_error.appendChild(error_message);
									$this
										.parent()
										.append(field_error);
									$this
										.css({"border-color":"#d43f3a"})
										.addClass("has-error");
								}
								if (onIncomplete) {
									onIncomplete(this);
								}
								/* stop */
								return;
							}
						}
					}
					success++;
				}
				var ip = ip_fields_array[0]+'.'+ip_fields_array[1]+'.'+ip_fields_array[2]+'.'+ip_fields_array[3];
				$this.val(ip);
				if(success == array_length) {
					if(onSuccess) {
						onSuccess(this, ip);
					}
				}
			}
		} else {
			if(empty_check == 1) {
				error_message = document.createTextNode(__empty_field);
				field_error = document.createElement("span");
				field_error.setAttribute("id", "field_error");
				field_error.appendChild(error_message);
				$this
					.parent()
					.append(field_error);
				$this
					.css({"border-color":"#d43f3a"})
					.addClass("has-error");
				if (onIncomplete) {
					onIncomplete (this);
				}
			}
		}
	});
	/* trigger the jquery focus event */
	$(inputID).on("focus", function() {
		/* remove all messages */
		$(this)
			.siblings('#field_error')
			.remove();
		$(this).css({"border-color":"#557Fb9"});
		$(this).removeClass("has-error");
	});
}

function validateEmptyField(inputID, onSuccess, onIncomplete)
{
	$(inputID).on("focusout", function() {
		var field_error;
		var error_message;
		var $this = $(this);
		var field = $this.val();

		field_error = document.createElement("span");
		field_error.setAttribute("id", "field_error");
		if(field) {
			if(onSuccess) {
				onSuccess(this, field);
			}
		} else {
			error_message = document.createTextNode(__empty_field);
			field_error.appendChild(error_message);
			$this
				.parent()
				.append(field_error);
			$this
				.css({"border-color":"#d43f3a"})
				.addClass("has-error");
			if (onIncomplete)
				onIncomplete(this);
		}
	});
	/* trigger the jquery focus event */
	$(inputID).on("focus", function() {
		/* remove all messages */
		$(this)
			.siblings('#field_error')
			.remove();
		$(this).css({"border-color":"#557Fb9"});
		$(this).removeClass("has-error");
	});
}

function validateMACField(inputID, empty_check, onSuccess, onIncomplete)
{
	$(inputID).on("focusout", function() {
		var field_error;
		var error_message;
		var $this = $(this);
		var mac = $this.val();

		field_error = document.createElement("span");
		field_error.setAttribute("id", "field_error");
		if(mac) {
			var mac_array = mac.split(":");
			if(mac_array.length < 6) {
				error_message = document.createTextNode(__incorrect_mac);
				field_error.appendChild(error_message);
				$this
					.parent()
					.append(field_error);
				$this
					.css({"border-color":"#d43f3a"})
					.addClass("has-error");
				if (onIncomplete) {
					onIncomplete(this);
				}
				/* stop */
				return;
			}
			for(var i=0; i<mac_array.length; i++) {
				if(mac_array[i].replace(/ /g, '').length < 2) {
					error_message = document.createTextNode(__incorrect_mac);
					field_error.appendChild(error_message);
					$this
						.parent()
						.append(field_error);
					$this
						.css({"border-color":"#d43f3a"})
						.addClass("has-error");
					if (onIncomplete) {
						onIncomplete(this);
					}
					/* stop */
					return;
				}
			}

			if(onSuccess) {
				onSuccess(this, mac);
			}
		} else {
			if(empty_check == 1) {
				error_message = document.createTextNode(__empty_field);
				field_error.appendChild(error_message);
				$this
					.parent()
					.append(field_error);
				$this
					.css({"border-color":"#d43f3a"})
					.addClass("has-error");
				if (onIncomplete) {
					onIncomplete(this);
				}
			}
		}
	});
	/* trigger the jquery focus event */
	$(inputID).on("focus", function() {
		/* remove all messages */
		$(this)
			.siblings('#field_error')
			.remove();
		$(this).css({"border-color":"#557Fb9"});
		$(this).removeClass("has-error");
	});
}

function openModal(mycss)
{
	$('#fade').show();
	if (mycss) {
		$('#box').removeClass();
		$('#box').addClass('modal');
		$('#modal_pic_container').removeClass();
		$('#modal_pic_container').addClass(mycss);

		if (mycss == 'burning') {
			$('#burn_progress_box').show();
		} else {
			$('#burn_progress_box').hide();
		}
	} else {
		$('#box').removeClass();
		$('#box').addClass('modal_nobg');
		$('#modal_pic_container').removeClass();
		$('#modal_pic_container').addClass('loading_user');
		$('#burn_progress_box').hide();
	}

	var h = $(window).height();
	var w = $(window).width();
	var padding = $('#box').css('padding-left');
	var image_h = $('#box').height()+parseInt(padding)*2;
	var image_w = $('#box').width()+parseInt(padding)*2;
	var top = (h-image_h)/2;
	var left = (w-image_w)/2;
	$('#box').css({top:top, left:left, position:'absolute'});
	$('#box').show();
}

function delayclose()
{
	$('#box').fadeOut(500);
	$('#fade').fadeOut(500);
	$('#burn_progress_box').fadeOut(500);
}

function closeModal()
{
	setTimeout(delayclose, 200);
}

function show_alert(title, msg, timeout, w, h, type)
{
	var ctime, cw, ch ,ct;
	
	if (!timeout)
		ctime = 1500;
	else
		ctime = timeout;
	if (!w)
		cw = 300;
	else
		cw = w;
	if (!h)
		ch = 200;
	else	
		ch = h;
	if (!type)
		ct = 'slide';
	else
		ct = type;
	$.messager.show({
		title: title,
		msg: msg,
		timeout: ctime,
		width: cw,
		height: ch,
		showType: ct
	});
}

function attach_errormsg(input_obj, msg_obj, error_txt)
{
	var field_error;
	var error_message;

	detach_errormsg(input_obj, msg_obj);

	field_error = document.createElement('span');
	field_error.setAttribute('id', 'field_error');
	error_message = document.createTextNode(error_txt);
	field_error.appendChild(error_message);
	if (msg_obj)
		msg_obj.append(field_error);

	if (input_obj) {
		input_obj.css({'border-color':'#d43f3a'});
		input_obj.addClass('has-error');
	}
}

function detach_errormsg(input_obj, msg_obj)
{
	if (msg_obj)
		msg_obj.find('#field_error').remove();

	if (input_obj) {
		input_obj.css({'border-color':'#557Fb9'});
		input_obj.removeClass('has-error');
	}
}

function init_tz_str(id)
{
	if (typeof __tz_UCT__11 == 'string') {
		if (typeof id == 'string' && id.length > 0) {
			$('#'+id).empty();
			$('#'+id).append($('<option value="UCT_-11">(GMT-11:00) '+__tz_UCT__11+'</option>'));
			$('#'+id).append($('<option value="UCT_-10">(GMT-10:00) '+__tz_UCT__10+'</option>'));
			$('#'+id).append($('<option value="NAS_-09">(GMT-09:00) '+__tz_NAS__09+'</option>'));
			$('#'+id).append($('<option value="PST_-08">(GMT-08:00) '+__tz_PST__08+'</option>'));
			$('#'+id).append($('<option value="MST_-07">(GMT-07:00) '+__tz_MST__07+'</option>'));
			$('#'+id).append($('<option value="CST_-06">(GMT-06:00) '+__tz_CST__06+'</option>'));
			$('#'+id).append($('<option value="UCT_-06">(GMT-06:00) '+__tz_UCT__06+'</option>'));
			$('#'+id).append($('<option value="UCT_-05">(GMT-05:00) '+__tz_UCT__05+'</option>'));
			$('#'+id).append($('<option value="EST_-05">(GMT-05:00) '+__tz_EST__05+'</option>'));
			$('#'+id).append($('<option value="AST_-04">(GMT-04:00) '+__tz_AST__04+'</option>'));
			$('#'+id).append($('<option value="SAT_-03">(GMT-03:00) '+__tz_SAT__03+'</option>'));
			$('#'+id).append($('<option value="EBS_-03">(GMT-03:00) '+__tz_EBS__03+'</option>'));
			$('#'+id).append($('<option value="NOR_-02">(GMT-02:00) '+__tz_NOR__02+'</option>'));
			$('#'+id).append($('<option value="EUT_-01">(GMT-01:00) '+__tz_EUT__01+'</option>'));
			$('#'+id).append($('<option value="UCT_000">(GMT)       '+__tz_UCT_000+'</option>'));
			$('#'+id).append($('<option value="GMT_000">(GMT)       '+__tz_GMT_000+'</option>'));
			$('#'+id).append($('<option value="CET_001">(GMT+01:00) '+__tz_CET_001+'</option>'));
			$('#'+id).append($('<option value="MEZ_001">(GMT+01:00) '+__tz_MEZ_001+'</option>'));
			$('#'+id).append($('<option value="EET_002">(GMT+02:00) '+__tz_EET_002+'</option>'));
			$('#'+id).append($('<option value="SAS_002">(GMT+02:00) '+__tz_SAS_002+'</option>'));
			$('#'+id).append($('<option value="IST_003">(GMT+03:00) '+__tz_IST_003+'</option>'));
			$('#'+id).append($('<option value="MSK_003">(GMT+03:00) '+__tz_MSK_003+'</option>'));
			$('#'+id).append($('<option value="UCT_004">(GMT+04:00) '+__tz_UCT_004+'</option>'));
			$('#'+id).append($('<option value="UCT_005">(GMT+05:00) '+__tz_UCT_005+'</option>'));
			$('#'+id).append($('<option value="UCT_006">(GMT+06:00) '+__tz_UCT_006+'</option>'));
			$('#'+id).append($('<option value="UCT_007">(GMT+07:00) '+__tz_UCT_007+'</option>'));
			$('#'+id).append($('<option value="CST_008">(GMT+08:00) '+__tz_CST_008+'</option>'));
			$('#'+id).append($('<option value="CCT_008">(GMT+08:00) '+__tz_CCT_008+'</option>'));
			$('#'+id).append($('<option value="SST_008">(GMT+08:00) '+__tz_SST_008+'</option>'));
			$('#'+id).append($('<option value="AWS_008">(GMT+08:00) '+__tz_AWS_008+'</option>'));
			$('#'+id).append($('<option value="JST_009">(GMT+09:00) '+__tz_JST_009+'</option>'));
			$('#'+id).append($('<option value="UCT_010">(GMT+10:00) '+__tz_UCT_010+'</option>'));
			$('#'+id).append($('<option value="AES_010">(GMT+10:00) '+__tz_AES_010+'</option>'));
			$('#'+id).append($('<option value="UCT_011">(GMT+11:00) '+__tz_UCT_011+'</option>'));
			$('#'+id).append($('<option value="UCT_012">(GMT+12:00) '+__tz_UCT_012+'</option>'));
			$('#'+id).append($('<option value="NZS_012">(GMT+12:00) '+__tz_NZS_012+'</option>'));
		} else {
			$('#time_zone').empty();
			$('#time_zone').append($('<option value="UCT_-11">(GMT-11:00) '+__tz_UCT__11+'</option>'));
			$('#time_zone').append($('<option value="UCT_-10">(GMT-10:00) '+__tz_UCT__10+'</option>'));
			$('#time_zone').append($('<option value="NAS_-09">(GMT-09:00) '+__tz_NAS__09+'</option>'));
			$('#time_zone').append($('<option value="PST_-08">(GMT-08:00) '+__tz_PST__08+'</option>'));
			$('#time_zone').append($('<option value="MST_-07">(GMT-07:00) '+__tz_MST__07+'</option>'));
			$('#time_zone').append($('<option value="CST_-06">(GMT-06:00) '+__tz_CST__06+'</option>'));
			$('#time_zone').append($('<option value="UCT_-06">(GMT-06:00) '+__tz_UCT__06+'</option>'));
			$('#time_zone').append($('<option value="UCT_-05">(GMT-05:00) '+__tz_UCT__05+'</option>'));
			$('#time_zone').append($('<option value="EST_-05">(GMT-05:00) '+__tz_EST__05+'</option>'));
			$('#time_zone').append($('<option value="AST_-04">(GMT-04:00) '+__tz_AST__04+'</option>'));
			$('#time_zone').append($('<option value="SAT_-03">(GMT-03:00) '+__tz_SAT__03+'</option>'));
			$('#time_zone').append($('<option value="EBS_-03">(GMT-03:00) '+__tz_EBS__03+'</option>'));
			$('#time_zone').append($('<option value="NOR_-02">(GMT-02:00) '+__tz_NOR__02+'</option>'));
			$('#time_zone').append($('<option value="EUT_-01">(GMT-01:00) '+__tz_EUT__01+'</option>'));
			$('#time_zone').append($('<option value="UCT_000">(GMT)       '+__tz_UCT_000+'</option>'));
			$('#time_zone').append($('<option value="GMT_000">(GMT)       '+__tz_GMT_000+'</option>'));
			$('#time_zone').append($('<option value="CET_001">(GMT+01:00) '+__tz_CET_001+'</option>'));
			$('#time_zone').append($('<option value="MEZ_001">(GMT+01:00) '+__tz_MEZ_001+'</option>'));
			$('#time_zone').append($('<option value="EET_002">(GMT+02:00) '+__tz_EET_002+'</option>'));
			$('#time_zone').append($('<option value="SAS_002">(GMT+02:00) '+__tz_SAS_002+'</option>'));
			$('#time_zone').append($('<option value="IST_003">(GMT+03:00) '+__tz_IST_003+'</option>'));
			$('#time_zone').append($('<option value="MSK_003">(GMT+03:00) '+__tz_MSK_003+'</option>'));
			$('#time_zone').append($('<option value="UCT_004">(GMT+04:00) '+__tz_UCT_004+'</option>'));
			$('#time_zone').append($('<option value="UCT_005">(GMT+05:00) '+__tz_UCT_005+'</option>'));
			$('#time_zone').append($('<option value="UCT_006">(GMT+06:00) '+__tz_UCT_006+'</option>'));
			$('#time_zone').append($('<option value="UCT_007">(GMT+07:00) '+__tz_UCT_007+'</option>'));
			$('#time_zone').append($('<option value="CST_008">(GMT+08:00) '+__tz_CST_008+'</option>'));
			$('#time_zone').append($('<option value="CCT_008">(GMT+08:00) '+__tz_CCT_008+'</option>'));
			$('#time_zone').append($('<option value="SST_008">(GMT+08:00) '+__tz_SST_008+'</option>'));
			$('#time_zone').append($('<option value="AWS_008">(GMT+08:00) '+__tz_AWS_008+'</option>'));
			$('#time_zone').append($('<option value="JST_009">(GMT+09:00) '+__tz_JST_009+'</option>'));
			$('#time_zone').append($('<option value="UCT_010">(GMT+10:00) '+__tz_UCT_010+'</option>'));
			$('#time_zone').append($('<option value="AES_010">(GMT+10:00) '+__tz_AES_010+'</option>'));
			$('#time_zone').append($('<option value="UCT_011">(GMT+11:00) '+__tz_UCT_011+'</option>'));
			$('#time_zone').append($('<option value="UCT_012">(GMT+12:00) '+__tz_UCT_012+'</option>'));
			$('#time_zone').append($('<option value="NZS_012">(GMT+12:00) '+__tz_NZS_012+'</option>'));
		}
	}
}

function check_ip(entered_ip, mask)
{
	if (!entered_ip)
		return false;

	var success = 0;
	var ip_fields_array = entered_ip.split(".");
	var array_length = ip_fields_array.length;
	var regex = /^0([0-9])+/;

	if (array_length < 4)
		return false;

	for (var i = 0; i < array_length; i++) {
		var ip_field = ip_fields_array[i];

		ip_fields_array[i] = ip_fields_array[i].replace(/ /g, "");
		if(ip_fields_array[i].length > 1) {
			ip_field = ip_fields_array[i] = ip_fields_array[i].replace(/^0/g, "");
		}
		if(ip_fields_array[i].length > 1) {
			ip_field = ip_fields_array[i] = ip_fields_array[i].replace(/^0/g, "");
		}

		var regex_result = regex.test(ip_field.replace(/ /g, ""));
		if (!ip_field.length || ip_field == " " || ip_field == "   " || regex_result == true) {
			return false;
		} else {
			if(i == 0) {
				if(ip_field.replace(/ /g, "") == "0") {
					return false;
				}
			}
			if (mask == 0) {
				if(i == 3) {
					if(ip_field.replace(/ /g, "") == "0") {
						return false;
					}
				}
			}
			success++;
		}

		if(success == array_length) {
			return true;
		}
	}
	return false;
}

function check_mac(mac)
{
	var mac_array = mac.split(":");
	if(mac_array.length < 6) {
		return false;
	}

	for(var i=0; i<mac_array.length; i++) {
		if(mac_array[i].replace(/ /g, '').length < 2) {
			return false;
		}
	}

	return true;
}

function d2h(n)
{
	return parseInt(n, 10).toString(16);
}

function invert(n)
{
	return ~(parseInt(n)) & 0xFF;
}

function get_subnet(ip, mask, do_invert)
{
	var result;
	if (ip && mask) {
		var ip_digit = ip.split('.');
		var mask_digit = mask.split('.');
		var result_digit = [];

		if (check_ip(ip, 0) == false) {
			return '~~';
		}

		if (check_ip(mask, 1) == false) {
			return '~~';
		}

		if (!do_invert) {
			result_digit[0] = parseInt(ip_digit[0]) & parseInt(mask_digit[0]);
			result_digit[1] = parseInt(ip_digit[1]) & parseInt(mask_digit[1]);
			result_digit[2] = parseInt(ip_digit[2]) & parseInt(mask_digit[2]);
			result_digit[3] = parseInt(ip_digit[3]) & parseInt(mask_digit[3]);
		} else {
			result_digit[0] = parseInt(ip_digit[0]) & parseInt(invert(mask_digit[0]));
			result_digit[1] = parseInt(ip_digit[1]) & parseInt(invert(mask_digit[1]));
			result_digit[2] = parseInt(ip_digit[2]) & parseInt(invert(mask_digit[2]));
			result_digit[3] = parseInt(ip_digit[3]) & parseInt(invert(mask_digit[3]));
		}
		result = result_digit[0]+'.'+result_digit[1]+'.'+result_digit[2]+'.'+result_digit[3];
	} else 
		result = '0.0.0.0';

	return result;
}

function combine_ip(ip1, ip2)
{
	var result;
	if (ip1 && ip2) {
		var ip1_digit = ip1.split('.');
		var ip2_digit = ip2.split('.');
		var result_digit = [];

		result_digit[0] = parseInt(ip1_digit[0]) | parseInt(ip2_digit[0]);
		result_digit[1] = parseInt(ip1_digit[1]) | parseInt(ip2_digit[1]);
		result_digit[2] = parseInt(ip1_digit[2]) | parseInt(ip2_digit[2]);
		result_digit[3] = parseInt(ip1_digit[3]) | parseInt(ip2_digit[3]);
		result = result_digit[0]+'.'+result_digit[1]+'.'+result_digit[2]+'.'+result_digit[3];
	} else 
		result = '0.0.0.0';

	return result;
}

function check_wan_lan(ip1, mask1, ip2, mask2)
{
	var subnet1 = get_subnet(ip1, mask1);
	var subnet2 = get_subnet(ip2, mask2);

	if (subnet1 == '~~' || subnet2 == '~~' ||
	    subnet1 == '0.0.0.0' || subnet2 == '0.0.0.0')
		return false;

	if (subnet1 == subnet2) {
		return true;
	}

	return false;
}

function check_dhcpip_range(start, end, mask)
{
	var ipstart_a = start.split('.');
	var ipend_a = end.trim().split('.');
	var mask_a = mask.split('.');
	var ips = 0;
	var ipe = 0;
	var step = 3;

	for(var i=0; i<mask_a.length; i++) {
		ips += (ipstart_a[i] & invert(mask_a[i])) << ((step-i)*8);
		ipe += (ipend_a[i] & invert(mask_a[i])) << ((step-i)*8);
	}

	if (ips - ipe >= 0)
		return false;

	return true;
}

var next_msg_timer;
function remove_msg()
{
	$('#apply_error').remove();
}

function show_message(objID, text)
{
	clearTimeout(next_msg_timer);
	next_msg_timer = setTimeout(remove_msg, 5000);
	$('#apply_error').remove();
	var warning_field = document.createElement("span");
	var warning_msg = document.createTextNode(text);
	warning_field.setAttribute("id", "apply_error");
	warning_field.appendChild(warning_msg);
	$(objID).append(warning_field);
}

function resize_iframe(height)
{
	var h;
	if (height)
		h = height;
	else
		h = 870;
	var theFrame = $('iframe', parent.document.body);
	theFrame.each(function () {
		$(this).height(h);
	});
}

function scale_size(id, ratio)
{
	$('#'+id).css({ '-webkit-transform':'scale(' + ratio + ')',
			'-webkit-transform':'scale(' + ratio + ')',
			'-moz-transform'   :'scale(' + ratio + ')',
			'-ms-transform'    :'scale(' + ratio + ')',
			'-o-transform'     :'scale(' + ratio + ')',
			'transform'        :'scale(' + ratio + ')'});
}

function check_browser()
{
	var type = 0;
	/* Opera 8.0+ */
	var isOpera = (!!window.opr && !!opr.addons) || !!window.opera || navigator.userAgent.indexOf(' OPR/') >= 0;
	
	/* Firefox 1.0+ */
	var isFirefox = typeof InstallTrigger !== 'undefined';
	
	/* Safari 3.0+ "[object HTMLElementConstructor]" */
	var isSafari = /constructor/i.test(window.HTMLElement) || (function (p) { return p.toString() === "[object SafariRemoteNotification]"; })(!window['safari'] || safari.pushNotification);
	
	/* Internet Explorer 6-11 */
	var isIE = /*@cc_on!@*/false || !!document.documentMode;
	
	/* Edge 20+ */
	var isEdge = !isIE && !!window.StyleMedia;

	/* Chrome 1+ */
	var isChrome = !!window.chrome && !!window.chrome.webstore;

	if (isOpera == true)
		type = 1;
	else if (isFirefox == true)
		type = 2;
	else if (isSafari == true)
		type = 3;
	else if (isIE == true)
		type = 4;
	else if (isEdge == true)
		type = 5;
	else if (isChrome == true)
		type = 6;

	return type;
}

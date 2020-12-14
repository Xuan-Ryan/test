function set_wep_key_attr(input_id, type)
{
	if (type == 0) {
		/*  hex  */
		$(input_id).inputmask({
			'placeholder': '',
			'mask': '[*{10,26}]',
			definitions: {
				'*': {
					validator: '[0-9A-Fa-f]',
					cardinality: 1
				},
			},
		});
		$(input_id).attr('maxlength', '26');
	} else {
		/*  ascii  */
		$(input_id).inputmask({
			'placeholder': '',
			'mask': '[*{5,13}]',
			definitions: {
				'*': {
					validator: '[0-9A-Za-z]',
					cardinality: 1
				},
			},
		});
		$(input_id).attr('maxlength', '13');
	}
}

function validate_ip_addr(input_id, mask, empty_check, message_obj, onsuccess, onincomplete)
{
	$(input_id).on('focusout', function() {
		var success = 0;
		var $this = $(this);
		var entered_ip = $this.val();

		if (entered_ip) {
			var ip_fields_array = entered_ip.split('.');

			var ip_error = document.createElement('span');
			ip_error.setAttribute('id', 'ip_error');
			var error_message;

			var array_length = ip_fields_array.length;
			var regex = /^0([0-9])+/;

			for (var i = 0; i < array_length; i++) {
				var ip_field = ip_fields_array[i];

				ip_fields_array[i] = ip_fields_array[i].replace(/ /g, '');
				if (ip_fields_array[i].length > 1) {
					ip_field = ip_fields_array[i] = ip_fields_array[i].replace(/^0/g, '');
				}
				if (ip_fields_array[i].length > 1) {
					ip_field = ip_fields_array[i] = ip_fields_array[i].replace(/^0/g, '');
				}

				var regex_result = regex.test(ip_field.replace(/ /g, ''));

				if (ip_field == '   ' || regex_result == true) {
					if ($this.siblings('#ip_error').length == 0) {
						if (mask == 0)
							error_message = document.createTextNode(__incorrect_ip);
						else
							error_message = document.createTextNode(__incorrect_mask);
						ip_error.appendChild(error_message);
						if (message_obj)
							$(message_obj).append(ip_error);
						$this.css({'border-color':'#d43f3a'});
						$this.addClass('has-error');
						if (onincomplete)
							onincomplete(this);
					}
					return;
				} else {
					if (i == 0) {
						if (ip_field.replace(/ /g, '') == '0') {
							if ($this.siblings('#ip_error').length == 0) {
								error_message = document.createTextNode(__incorrect_first_ip);
								ip_error.appendChild(error_message);
								if (message_obj)
									$(message_obj).append(ip_error);
								$this.css({'border-color':'#d43f3a'});
								$this.addClass('has-error');
								if (onincomplete)
									onincomplete(this);
							}
							return;
						}
					}
					if (mask == 0) {
						if (i == 3) {
							if (ip_field.replace(/ /g, '') == '0') {
								if ($this.siblings('#ip_error').length == 0) {
									error_message = document.createTextNode(__incorrect_last_ip);
									ip_error.appendChild(error_message);
									if (message_obj)
										$(message_obj).append(ip_error);
									$this.css({'border-color':'#d43f3a'});
									$this.addClass('has-error');
									if (onincomplete)
										onincomplete(this);
								}
								return;
							}
						}
					}
					success++;
				}
				var ip = ip_fields_array[0]+'.'+ip_fields_array[1]+'.'+ip_fields_array[2]+'.'+ip_fields_array[3];
				$this.val(ip);
				if (success == array_length) {
					if (onsuccess) {
						onsuccess(this, ip);
					}
				}
			}
		} else {
			if (empty_check == 1) {
				error_message = document.createTextNode(__empty_field);
				ip_error = document.createElement('span');
				ip_error.setAttribute('id', 'ip_error');
				ip_error.appendChild(error_message);
				if (message_obj)
					$(message_obj).append(ip_error);
				$this.css({'border-color':'#d43f3a'});
				$this.addClass('has-error');
				if (onincomplete)
					onincomplete(this);
			}
		}
	});

	$(input_id).on('focus', function() {
		/* remove all messages */
		if (message_obj)
			$(message_obj).find('span').remove();
		$(this).css({'border-color':'#557Fb9'});
		$(this).removeClass('has-error');
	});
}

function validate_security_format(input_id, onsuccess, onincomplete)
{
	$(input_id).on('focusout', function() {
		var field_error;
		var error_message;
		var $this = $(this);
		var field = $this.val().length;

		field_error = document.createElement('span');
		field_error.setAttribute('id', 'field_error');

		if (input_id.match('Key1Str')) {
			if ($this.val() == 0) {
				/*format is HEX*/
				if ((field == 10) || (field == 26)) {
					if (onsuccess) {
						onsuccess(this, field);
					}
				} else {
					if ((field != 10) && (field != 26))
						error_message = document.createTextNode(__correct_hex_length);
					field_error.appendChild(error_message);

					$('#wep_err_msg').append(field_error);
					$this.css({'border-color':'#d43f3a'});
					$this.addClass('has-error');

					if (onincomplete)
						onincomplete(this);
				}
			} else {
				/*format is ASCII*/
				if ((field == 5) || (field == 13)) {
					if (onsuccess) {
						onsuccess(this, field);
					}
				} else {
					if ((field != 5) && (field != 13))
						error_message = document.createTextNode(__correct_ascii_length);
					field_error.appendChild(error_message);

					$('#wep_err_msg').append(field_error);
					$this.css({'border-color':'#d43f3a'});
					$this.addClass('has-error');

					if (onincomplete)
						onincomplete(this);
				}
			}
		} else if (input_id.match('wpa_psk_key')) {
			if (field >= 8) {
				if (onsuccess) {
					onsuccess(this, field);
				}
			} else {
				if (field == 0)
					error_message = document.createTextNode(__empty_field);
				else
					error_message = document.createTextNode(__more_than_8characters);
				field_error.appendChild(error_message);
				$('#wpa_psk_err_msg').append(field_error);
				$this.css({'border-color':'#d43f3a'});
				$this.addClass('has-error');

				if (onincomplete)
					onincomplete(this);
			}
		} else if (input_id.match('keyRenewalInterval')) {
			if (field > 0) {
				if (parseInt($this.val()) < 1800)
					$this.val(1800);
				if (onsuccess) {
					onsuccess(this, field);
				}
			} else {
				error_message = document.createTextNode(__empty_field);
				field_error.appendChild(error_message);
				$('#keyRenewal_err_msg').append(field_error);
				$this.css({'border-color':'#d43f3a'});
				$this.addClass('has-error');

				if (onincomplete)
					onincomplete(this);
			}
		}
	});

	$(input_id).on('focus', function() {
		if (input_id.match('Key1Str'))
			$('#wep_err_msg').find('span').remove();
		else if (input_id.match('wpa_psk_key'))
			$('#wpa_psk_err_msg').find('span').remove();
		else if (input_id.match('eyRenewalInterval'))
			$('#keyRenewal_err_msg').find('span').remove();

		$(this).css({'border-color':'#557Fb9'});
		$(this).removeClass('has-error');
	});
}

function init_country_list()
{
	$('[id=__al]').text(__al);
	$('[id=__dz]').text(__dz);
	$('[id=__ar]').text(__ar);
	$('[id=__au]').text(__au);
	$('[id=__at]').text(__at);
	$('[id=__bd]').text(__bd);
	$('[id=__by]').text(__by);
	$('[id=__be]').text(__be);
	$('[id=__bo]').text(__bo);
	$('[id=__br]').text(__br);
	$('[id=__bg]').text(__bg);
	$('[id=__ca]').text(__ca);
	$('[id=__cl]').text(__cl);
	$('[id=__cn]').text(__cn);
	$('[id=__co]').text(__co);
	$('[id=__cr]').text(__cr);
	$('[id=__hr]').text(__hr);
	$('[id=__cy]').text(__cy);
	$('[id=__cz]').text(__cz);
	$('[id=__dk]').text(__dk);
	$('[id=__do]').text(__do);
	$('[id=__ec]').text(__ec);
	$('[id=__eg]').text(__eg);
	$('[id=__sv]').text(__sv);
	$('[id=__ee]').text(__ee);
	$('[id=__fi]').text(__fi);
	$('[id=__fr]').text(__fr);
	$('[id=__ge]').text(__ge);
	$('[id=__de]').text(__de);
	$('[id=__gr]').text(__gr);
	$('[id=__gt]').text(__gt);
	$('[id=__hn]').text(__hn);
	$('[id=__hk]').text(__hk);
	$('[id=__hu]').text(__hu);
	$('[id=__is]').text(__is);
	$('[id=__in]').text(__in);
	$('[id=__id]').text(__id);
	$('[id=__iq]').text(__iq);
	$('[id=__ie]').text(__ie);
	$('[id=__il]').text(__il);
	$('[id=__it]').text(__it);
	$('[id=__jp]').text(__jp);
	$('[id=__jo]').text(__jo);
	$('[id=__kz]').text(__kz);
	$('[id=__kr]').text(__kr);
	$('[id=__kw]').text(__kw);
	$('[id=__lv]').text(__lv);
	$('[id=__lb]').text(__lb);
	$('[id=__lt]').text(__lt);
	$('[id=__lu]').text(__lu);
	$('[id=__mo]').text(__mo);
	$('[id=__mk]').text(__mk);
	$('[id=__my]').text(__my);
	$('[id=__mx]').text(__mx);
	$('[id=__mc]').text(__mc);
	$('[id=__ma]').text(__ma);
	$('[id=__nl]').text(__nl);
	$('[id=__nz]').text(__nz);
	$('[id=__ng]').text(__ng);
	$('[id=__no]').text(__no);
	$('[id=__om]').text(__om);
	$('[id=__pk]').text(__pk);
	$('[id=__pa]').text(__pa);
	$('[id=__pe]').text(__pe);
	$('[id=__ph]').text(__ph);
	$('[id=__pl]').text(__pl);
	$('[id=__pt]').text(__pt);
	$('[id=__pr]').text(__pr);
	$('[id=__ro]').text(__ro);
	$('[id=__ru]').text(__ru);
	$('[id=__sa]').text(__sa);
	$('[id=__sg]').text(__sg);
	$('[id=__sk]').text(__sk);
	$('[id=__sl]').text(__sl);
	$('[id=__za]').text(__za);
	$('[id=__es]').text(__es);
	$('[id=__se]').text(__se);
	$('[id=__ch]').text(__ch);
	$('[id=__tw]').text(__tw);
	$('[id=__th]').text(__th);
	$('[id=__tr]').text(__tr);
	$('[id=__ua]').text(__ua);
	$('[id=__ae]').text(__ae);
	$('[id=__gb]').text(__gb);
	$('[id=__us]').text(__us);
	$('[id=__uy]').text(__uy);
	$('[id=__uz]').text(__uz);
	$('[id=__ve]').text(__ve);
	$('[id=__vn]').text(__vn);
	$('[id=__none]').text(__none);
}

function code2reg_24g(code)
{
	switch (code) {
	case 'AR':
	case 'BR':
	case 'CA':
	case 'HR':
	case 'DO':
	case 'GT':
	case 'HK':
	case 'MY':
	case 'MX':
	case 'PA':
	case 'PE':
	case 'PR':
	case 'TW':
	case 'US':
		return 0;
	case 'AL':
	case 'DZ':
	case 'AU':
	case 'AT':
	case 'BD':
	case 'BY':
	case 'BE':
	case 'BO':
	case 'BG':
	case 'CL':
	case 'CN':
	case 'CO':
	case 'CR':
	case 'CY':
	case 'CZ':
	case 'DK':
	case 'EC':
	case 'EG':
	case 'SV':
	case 'EE':
	case 'FI':
	case 'FR':
	case 'GE':
	case 'DE':
	case 'GR':
	case 'HN':
	case 'HU':
	case 'IS':
	case 'IN':
	case 'ID':
	case 'IQ':
	case 'IE':
	case 'IT':
	case 'JP':
	case 'JO':
	case 'KZ':
	case 'KR':
	case 'KW':
	case 'LV':
	case 'LB':
	case 'LT':
	case 'LU':
	case 'MO':
	case 'MK':
	case 'MC':
	case 'MA':
	case 'NL':
	case 'NZ':
	case 'NG':
	case 'NO':
	case 'OM':
	case 'PK':
	case 'PH':
	case 'PL':
	case 'PT':
	case 'RO':
	case 'RU':
	case 'SA':
	case 'SG':
	case 'SK':
	case 'SL':
	case 'ZA':
	case 'ES':
	case 'SE':
	case 'CH':
	case 'TH':
	case 'TR':
	case 'AE':
	case 'GB':
	case 'UY':
	case 'UZ':
	case 'VE':
	case 'VN':
	case 'IL':
		return 1;
	}
	return 0;
}

function code2reg_5g(code)
{
	switch(code) {
	case 'AR':
        case 'AU':
		return 10;  /*  ORIGINAL IS '0'  */
	case 'AT':
	case 'BY':
	case 'BE':
	case 'BG':
	case 'HR':
	case 'CY':
	case 'CZ':
	case 'DK':
	case 'EE':
	case 'FI':
	case 'FR':
	case 'DE':
	case 'GR':
	case 'HU':
	case 'IS':
	case 'IE':
	case 'IT':
	case 'JP':
	case 'LV':
	case 'LT':
	case 'LU':
	case 'MY':
	case 'NL':
	case 'NO':
	case 'OM':
	case 'PL':
	case 'PT':
	case 'RO':
	case 'SK':
	case 'SL':
	case 'ZA':
	case 'ES':
	case 'SE':
	case 'CH':
	case 'TR':
	case 'UA':
	case 'AE':
	case 'GB':
		return 6;  /*  ORIGINAL IS '1'  */
	case 'BD':
	case 'TW':
	case 'CN':
	case 'HN':
	case 'PK':
		return 4;
	case 'MO':
		return 5;
	case 'GE':
	case 'IQ':
	case 'IL':
	case 'JO':
	case 'KW':
	case 'MK':
	case 'MX':
	case 'MC':
	case 'MA':
		return 6;
	case 'BR':
	case 'CA':
	case 'CL':
	case 'CO':
	case 'CR':
	case 'DO':
	case 'EC':
	case 'EG':
	case 'KR':
	case 'SV':
	case 'GT':
	case 'HK':
	case 'IN':
	case 'LB':
	case 'NZ':
	case 'PA':
	case 'PE':
	case 'PH':
	case 'PR':
	case 'RU':
	case 'SA':
	case 'SG':
	case 'TH':
	case 'US':
	case 'UY':
	case 'UZ':
	case 'VE':
	case 'VN':
		return 10;
	}
	return 6;
}

function GetCountryCode()
{
	var lang_country = window.navigator.userLanguage || window.navigator.language;
	var browser_country_code = lang_country.split('-')[1];

	return browser_country_code;
}

function show_channel_list(id_name, region)
{
	/*  clear items  */
	$(id_name).empty();
	/*  add autoselect item  */
	$(id_name).append($('<option value="0" id="autoselect">'+__autoselect+'</option>'));

	if (region == 0 || region == 1) {
		$(id_name).append($('<option value=1  id="ch_1">2412MHz ('+__ch_+' 1)</option>'));
		$(id_name).append($('<option value=2  id="ch_2">2417MHz ('+__ch_+' 2)</option>'));
		$(id_name).append($('<option value=3  id="ch_3">2422MHz ('+__ch_+' 3)</option>'));
		$(id_name).append($('<option value=4  id="ch_4">2427MHz ('+__ch_+' 4)</option>'));
		$(id_name).append($('<option value=5  id="ch_5">2432MHz ('+__ch_+' 5)</option>'));
		$(id_name).append($('<option value=6  id="ch_6">2437MHz ('+__ch_+' 6)</option>'));
		$(id_name).append($('<option value=7  id="ch_7">2442MHz ('+__ch_+' 7)</option>'));
		$(id_name).append($('<option value=8  id="ch_8">2447MHz ('+__ch_+' 8)</option>'));
		$(id_name).append($('<option value=9  id="ch_9">2452MHz ('+__ch_+' 9)</option>'));
		$(id_name).append($('<option value=10 id="ch_10">2457MHz ('+__ch_+' 10)</option>'));
		$(id_name).append($('<option value=11 id="ch_11">2462MHz ('+__ch_+' 11)</option>'));
		if (region == 1) {
			$(id_name).append($('<option value=12 id="ch_12">2467MHz ('+__ch_+' 12)</option>'));
			$(id_name).append($('<option value=13 id="ch_13">2472MHz ('+__ch_+' 13)</option>'));
		}
	} else if (region == 2 || region == 3) {
		$(id_name).append($('<option value=10 id="ch_10">2457MHz ('+__ch_+' 10)</option>'));
		$(id_name).append($('<option value=11 id="ch_11">2462MHz ('+__ch_+' 11)</option>'));
		if (region == 3) {
			$(id_name).append($('<option value=12 id="ch_12">2467MHz ('+__ch_+' 12)</option>'));
			$(id_name).append($('<option value=13 id="ch_13">2472MHz ('+__ch_+' 13)</option>'));
		}
	} else if (region == 6) {
		$(id_name).append($('<option value=3  id="ch_3">2422MHz ('+__ch_+' 3)</option>'));
		$(id_name).append($('<option value=4  id="ch_4">2427MHz ('+__ch_+' 4)</option>'));
		$(id_name).append($('<option value=5  id="ch_5">2432MHz ('+__ch_+' 5)</option>'));
		$(id_name).append($('<option value=6  id="ch_6">2437MHz ('+__ch_+' 6)</option>'));
		$(id_name).append($('<option value=7  id="ch_7">2442MHz ('+__ch_+' 7)</option>'));
		$(id_name).append($('<option value=8  id="ch_8">2447MHz ('+__ch_+' 8)</option>'));
		$(id_name).append($('<option value=9  id="ch_9">2452MHz ('+__ch_+' 9)</option>'));
	} else if (region == 7) {
		$(id_name).append($('<option value=5  id="ch_5">2432MHz ('+__ch_+' 5)</option>'));
		$(id_name).append($('<option value=6  id="ch_6">2437MHz ('+__ch_+' 6)</option>'));
		$(id_name).append($('<option value=7  id="ch_7">2442MHz ('+__ch_+' 7)</option>'));
		$(id_name).append($('<option value=8  id="ch_8">2447MHz ('+__ch_+' 8)</option>'));
		$(id_name).append($('<option value=9  id="ch_9">2452MHz ('+__ch_+' 9)</option>'));
		$(id_name).append($('<option value=10 id="ch_10">2457MHz ('+__ch_+' 10)</option>'));
		$(id_name).append($('<option value=11 id="ch_11">2462MHz ('+__ch_+' 11)</option>'));
		$(id_name).append($('<option value=12 id="ch_12">2467MHz ('+__ch_+' 12)</option>'));
		$(id_name).append($('<option value=13 id="ch_13">2472MHz ('+__ch_+' 13)</option>'));
	} else {
		$(id_name).append($('<option value=1  id="ch_1">2412MHz ('+__ch_+' 1)</option>'));
		$(id_name).append($('<option value=2  id="ch_2">2417MHz ('+__ch_+' 2)</option>'));
		$(id_name).append($('<option value=3  id="ch_3">2422MHz ('+__ch_+' 3)</option>'));
		$(id_name).append($('<option value=4  id="ch_4">2427MHz ('+__ch_+' 4)</option>'));
		$(id_name).append($('<option value=5  id="ch_5">2432MHz ('+__ch_+' 5)</option>'));
		$(id_name).append($('<option value=6  id="ch_6">2437MHz ('+__ch_+' 6)</option>'));
		$(id_name).append($('<option value=7  id="ch_7">2442MHz ('+__ch_+' 7)</option>'));
		$(id_name).append($('<option value=8  id="ch_8">2447MHz ('+__ch_+' 8)</option>'));
		$(id_name).append($('<option value=9  id="ch_9">2452MHz ('+__ch_+' 9)</option>'));
		$(id_name).append($('<option value=10  id="ch_10">2457MHz ('+__ch_+' 10)</option>'));
		$(id_name).append($('<option value=11  id="ch_11">2462MHz ('+__ch_+' 11)</option>'));
	}
}

function show_channel_list_5g(id_name, region)
{
	/*  clear items  */
	$(id_name).empty();
	/*  add autoselect item  */
	$(id_name).append($('<option value="0" id="autoselect">'+__autoselect+'</option>'));
	if (region == 0) {
		$(id_name).append($('<option value="36" id="ch_36">5180MHZ ('+__ch_+' 36)</option>'));
		$(id_name).append($('<option value="40" id="ch_40">5200MHZ ('+__ch_+' 40)</option>'));
		$(id_name).append($('<option value="44" id="ch_44">5220MHZ ('+__ch_+' 44)</option>'));
		$(id_name).append($('<option value="48" id="ch_48">5240MHZ ('+__ch_+' 48)</option>'));
		$(id_name).append($('<option value="52" id="ch_52">5260MHZ ('+__ch_+' 52)</option>'));
		$(id_name).append($('<option value="56" id="ch_56">5280MHZ ('+__ch_+' 56)</option>'));
		$(id_name).append($('<option value="60" id="ch_60">5300MHZ ('+__ch_+' 60)</option>'));
		$(id_name).append($('<option value="64" id="ch_64">5320MHZ ('+__ch_+' 64)</option>'));
		$(id_name).append($('<option value="149" id="ch_149">5745MHZ ('+__ch_+' 149)</option>'));
		$(id_name).append($('<option value="153" id="ch_153">5765MHZ ('+__ch_+' 153)</option>'));
		$(id_name).append($('<option value="157" id="ch_157">5785MHZ ('+__ch_+' 157)</option>'));
		$(id_name).append($('<option value="161" id="ch_161">5805MHZ ('+__ch_+' 161)</option>'));
		$(id_name).append($('<option value="165" id="ch_165">5825MHZ ('+__ch_+' 165)</option>'));
	} else if (region == 1) {
		$(id_name).append($('<option value="36" id="ch_36">5180MHZ ('+__ch_+' 36)</option>'));
		$(id_name).append($('<option value="40" id="ch_40">5200MHZ ('+__ch_+' 40)</option>'));
		$(id_name).append($('<option value="44" id="ch_44">5220MHZ ('+__ch_+' 44)</option>'));
		$(id_name).append($('<option value="48" id="ch_48">5240MHZ ('+__ch_+' 48)</option>'));
		$(id_name).append($('<option value="52" id="ch_52">5260MHZ ('+__ch_+' 52)</option>'));
		$(id_name).append($('<option value="56" id="ch_56">5280MHZ ('+__ch_+' 56)</option>'));
		$(id_name).append($('<option value="60" id="ch_60">5300MHZ ('+__ch_+' 60)</option>'));
		$(id_name).append($('<option value="64" id="ch_64">5320MHZ ('+__ch_+' 64)</option>'));
		$(id_name).append($('<option value="100" id="ch_100">5500MHZ ('+__ch_+' 100)</option>'));
		$(id_name).append($('<option value="104" id="ch_104">5520MHZ ('+__ch_+' 104)</option>'));
		$(id_name).append($('<option value="108" id="ch_108">5540MHZ ('+__ch_+' 108)</option>'));
		$(id_name).append($('<option value="112" id="ch_112">5560MHZ ('+__ch_+' 112)</option>'));
		$(id_name).append($('<option value="116" id="ch_116">5580MHZ ('+__ch_+' 116)</option>'));
		$(id_name).append($('<option value="120" id="ch_120">5600MHZ ('+__ch_+' 120)</option>'));
		$(id_name).append($('<option value="124" id="ch_124">5620MHZ ('+__ch_+' 124)</option>'));
		$(id_name).append($('<option value="128" id="ch_128">5640MHZ ('+__ch_+' 128)</option>'));
		$(id_name).append($('<option value="132" id="ch_132">5660MHZ ('+__ch_+' 132)</option>'));
		$(id_name).append($('<option value="136" id="ch_136">5680MHZ ('+__ch_+' 136)</option>'));
		$(id_name).append($('<option value="140" id="ch_140">5700MHZ ('+__ch_+' 140)</option>'));
	} else if (region == 4) {
		$(id_name).append($('<option value="149" id="ch_149">5745MHZ ('+__ch_+' 149)</option>'));
		$(id_name).append($('<option value="153" id="ch_153">5765MHZ ('+__ch_+' 153)</option>'));
		$(id_name).append($('<option value="157" id="ch_157">5785MHZ ('+__ch_+' 157)</option>'));
		$(id_name).append($('<option value="161" id="ch_161">5805MHZ ('+__ch_+' 161)</option>'));
		$(id_name).append($('<option value="165" id="ch_165">5825MHZ ('+__ch_+' 165)</option>'));
	} else if (region == 5) {
		$(id_name).append($('<option value="149" id="ch_149">5745MHZ ('+__ch_+' 149)</option>'));
		$(id_name).append($('<option value="153" id="ch_153">5765MHZ ('+__ch_+' 153)</option>'));
		$(id_name).append($('<option value="157" id="ch_157">5785MHZ ('+__ch_+' 157)</option>'));
		$(id_name).append($('<option value="161" id="ch_161">5805MHZ ('+__ch_+' 161)</option>'));
	} else if (region == 6) {
		$(id_name).append($('<option value="36" id="ch_36">5180MHZ ('+__ch_+' 36)</option>'));
		$(id_name).append($('<option value="40" id="ch_40">5200MHZ ('+__ch_+' 40)</option>'));
		$(id_name).append($('<option value="44" id="ch_44">5220MHZ ('+__ch_+' 44)</option>'));
		$(id_name).append($('<option value="48" id="ch_48">5240MHZ ('+__ch_+' 48)</option>'));
	} else if (region == 10) {
		$(id_name).append($('<option value="36" id="ch_36">5180MHZ ('+__ch_+' 36)</option>'));
		$(id_name).append($('<option value="40" id="ch_40">5200MHZ ('+__ch_+' 40)</option>'));
		$(id_name).append($('<option value="44" id="ch_44">5220MHZ ('+__ch_+' 44)</option>'));
		$(id_name).append($('<option value="48" id="ch_48">5240MHZ ('+__ch_+' 48)</option>'));
		$(id_name).append($('<option value="149" id="ch_149">5745MHZ ('+__ch_+' 149)</option>'));
		$(id_name).append($('<option value="153" id="ch_153">5765MHZ ('+__ch_+' 153)</option>'));
		$(id_name).append($('<option value="157" id="ch_157">5785MHZ ('+__ch_+' 157)</option>'));
		$(id_name).append($('<option value="161" id="ch_161">5805MHZ ('+__ch_+' 161)</option>'));
		$(id_name).append($('<option value="165" id="ch_165">5825MHZ ('+__ch_+' 165)</option>'));
	} else {
		$(id_name).append($('<option value="36" id="ch_36">5180MHZ ('+__ch_+' 36)</option>'));
		$(id_name).append($('<option value="40" id="ch_40">5200MHZ ('+__ch_+' 40)</option>'));
		$(id_name).append($('<option value="44" id="ch_44">5220MHZ ('+__ch_+' 44)</option>'));
		$(id_name).append($('<option value="48" id="ch_48">5240MHZ ('+__ch_+' 48)</option>'));
	}
}

var secs = -1;
var timerID = null;
var timerRunning = false;
var timeout = 1;
var delay = 1000;

function InitializeWPSTimer()
{
	/* Set the length of the timer, in seconds */
	/*timeout = default*/
	secs = timeout;
	StopWPSClock();
	StartWPSTimer();
}

function StopWPSClock()
{
	if(timerRunning)
		clearTimeout(timerID);
	timerRunning = false;
}

function StartWPSTimer()
{
	if (secs==0) {
		StopWPSClock();

		updateWPS();

		secs = timeout;
		StartWPSTimer();
	} else {
		self.status = secs;
		secs = secs - 1;
		timerRunning = true;
		timerID = self.setTimeout("StartWPSTimer()", delay);
	}
}

var ChLst_24G = new Array(14);
var HT5GExtCh = new Array(22);

function init_all_chlist()
{
	ChLst_24G[0] = '2412MHz('+__ch_+' 1)';
	ChLst_24G[1] = '2417MHz('+__ch_+' 2)';
	ChLst_24G[2] = '2422MHz('+__ch_+' 3)';
	ChLst_24G[3] = '2427MHz('+__ch_+' 4)';
	ChLst_24G[4] = '2432MHz('+__ch_+' 5)';
	ChLst_24G[5] = '2437MHz('+__ch_+' 6)';
	ChLst_24G[6] = '2442MHz('+__ch_+' 7)';
	ChLst_24G[7] = '2447MHz('+__ch_+' 8)';
	ChLst_24G[8] = '2452MHz('+__ch_+' 9)';
	ChLst_24G[9] = '2457MHz('+__ch_+' 10)';
	ChLst_24G[10] = '2462MHz('+__ch_+' 11)';
	ChLst_24G[11] = '2467MHz('+__ch_+' 12)';
	ChLst_24G[12] = '2472MHz('+__ch_+' 13)';
	ChLst_24G[13] = '2484MHz('+__ch_+' 14)';

	HT5GExtCh[0] = new Array(1, '5200MHz ('+__ch_+' 40)'); /*36's extension channel*/
	HT5GExtCh[1] = new Array(0, '5180MHz ('+__ch_+' 36)'); /*40's extension channel*/
	HT5GExtCh[2] = new Array(1, '5240MHz ('+__ch_+' 48)'); /*44's*/
	HT5GExtCh[3] = new Array(0, '5220MHz ('+__ch_+' 44)'); /*48's*/
	HT5GExtCh[4] = new Array(1, '5280MHz ('+__ch_+' 56)'); /*52's*/
	HT5GExtCh[5] = new Array(0, '5260MHz ('+__ch_+' 52)'); /*56's*/
	HT5GExtCh[6] = new Array(1, '5320MHz ('+__ch_+' 64)'); /*60's*/
	HT5GExtCh[7] = new Array(0, '5300MHz ('+__ch_+' 60)'); /*64's*/
	HT5GExtCh[8] = new Array(1, '5520MHz ('+__ch_+' 104)'); /*100's*/
	HT5GExtCh[9] = new Array(0, '5500MHz ('+__ch_+' 100)'); /*104's*/
	HT5GExtCh[10] = new Array(1, '5560MHz ('+__ch_+' 112)'); /*108's*/
	HT5GExtCh[11] = new Array(0, '5540MHz ('+__ch_+' 108)'); /*112's*/
	HT5GExtCh[12] = new Array(1, '5600MHz ('+__ch_+' 120)'); /*116's*/
	HT5GExtCh[13] = new Array(0, '5580MHz ('+__ch_+' 116)'); /*120's*/
	HT5GExtCh[14] = new Array(1, '5640MHz ('+__ch_+' 128)'); /*124's*/
	HT5GExtCh[15] = new Array(0, '5620MHz ('+__ch_+' 124)'); /*128's*/
	HT5GExtCh[16] = new Array(1, '5680MHz ('+__ch_+' 136)'); /*132's*/
	HT5GExtCh[17] = new Array(0, '5660MHz ('+__ch_+' 132)'); /*136's*/
	HT5GExtCh[18] = new Array(1, '5765MHz ('+__ch_+' 153)'); /*149's*/
	HT5GExtCh[19] = new Array(0, '5745MHz ('+__ch_+' 149)'); /*153's*/
	HT5GExtCh[20] = new Array(1, '5805MHz ('+__ch_+' 161)'); /*157's*/
	HT5GExtCh[21] = new Array(0, '5785MHz ('+__ch_+' 157)'); /*161's*/
}

function CreateExtChOpt(vChannel)
{
	$('#n_extcha').append($('<option value=1>'+ChLst_24G[vChannel-1]+'</option>'));
}

function InsExtChOpt(mode, CurrCh, OptLen)
{
	if (mode == 9 || mode == 8 || 
	    mode == 6 || mode == 11 || 
	    mode >= 14) {
		var length = $('#n_extcha option').length;
		if (length > 1) {
			/*  remove all option which great than 0  */
			$('#n_extcha').find('option:gt(0)').remove();
		}

		if (mode == 8 || mode == 11 || mode >= 14) {
			if (CurrCh >= 36 && CurrCh <= 64) {
				CurrCh /= 4;
				CurrCh -= 9;

				$('#n_extcha option:eq(0)').text(HT5GExtCh[CurrCh][1]);
				$('#n_extcha option:eq(0)').val(HT5GExtCh[CurrCh][0]);
				$('#n_extcha').val(HT5GExtCh[CurrCh][0]);
			} else if (CurrCh >= 100 && CurrCh <= 136) {
				CurrCh /= 4;
				CurrCh -= 17;

				$('#n_extcha option:eq(0)').text(HT5GExtCh[CurrCh][1]);
				$('#n_extcha option:eq(0)').val(HT5GExtCh[CurrCh][0]);
				$('#n_extcha').val(HT5GExtCh[CurrCh][0]);
			} else if (CurrCh >= 149 && CurrCh <= 161) {
				CurrCh -= 1;
				CurrCh /= 4;
				CurrCh -= 19;

				$('#n_extcha option:eq(0)').text(HT5GExtCh[CurrCh][1]);
				$('#n_extcha option:eq(0)').val(HT5GExtCh[CurrCh][0]);
				$('#n_extcha').val(HT5GExtCh[CurrCh][0]);
			} else {
				$('#n_extcha option:eq(0)').text(__autoselect);
				$('#n_extcha option:eq(0)').val(0);
				$('#n_extcha').val(0);
			}
		} else if (mode == 9 || mode == 6) {
			if ((CurrCh >= 1) && (CurrCh <= 4)) {
				$('#n_extcha option:eq(0)').text(ChLst_24G[CurrCh+4-1]);
				$('#n_extcha option:eq(0)').val(1);
				$('#n_extcha').val(1);
			} else if ((CurrCh >= 5) && (CurrCh <= 7)) {
				$('#n_extcha option:eq(0)').text(ChLst_24G[CurrCh-4-1]);
				$('#n_extcha option:eq(0)').val(0);
				CurrCh += 4;

				CreateExtChOpt(CurrCh);
				$('#n_extcha').val(1);
			} else if ((CurrCh >= 8) && (CurrCh <= 9)) {
				$('#n_extcha option:eq(0)').text(ChLst_24G[CurrCh-4-1]);
				$('#n_extcha option:eq(0)').val(0);
				$('#n_extcha').val(0);

				if (OptLen >= 14) {
					CurrCh += 4;
					CreateExtChOpt(CurrCh);
					$('#n_extcha').val(1);
				}
			} else if (CurrCh == 10) {
				$('#n_extcha option:eq(0)').text(ChLst_24G[CurrCh-4-1]);
				$('#n_extcha option:eq(0)').val(0);
				$('#n_extcha').val(0);

				if (OptLen > 14) {
					CurrCh += 4;
					CreateExtChOpt(CurrCh);
					$('#n_extcha').val(1);
				}
			} else if (CurrCh >= 11) {
				$('#n_extcha option:eq(0)').text(ChLst_24G[CurrCh-4-1]);
				$('#n_extcha option:eq(0)').val(0);
				$('#n_extcha').val(0);
			} else {
				$('#n_extcha option:eq(0)').text(__autoselect);
				$('#n_extcha option:eq(0)').val(0);
				$('#n_extcha').val(0);
			}
		}
	}
}

function getWscStatusStr(status)
{
	switch(status){
	case 0:
		return __wps_not_used;
	case 1:
		return __wps_idle;
	case 3: /*return 'Start WSC Process';*/
	case 4: /*return 'Received EAPOL-Start';*/
	case 5: /*return 'Sending EAP-Req(ID)';*/
	case 6: /*return 'Receive EAP-Rsp(ID)';*/
	case 7: /*return 'Receive EAP-Req with wrong WSC SMI Vendor Id';*/
	case 8: /*return 'Receive EAPReq with wrong WSC Vendor Type';*/
	case 9: /*return 'Sending EAP-Req(WSC_START)';*/
	case 10: /*return 'Send M1';*/
	case 11: /*return 'Received M1';*/
	case 12: /*return 'Send M2';*/
	case 13: /*return 'Received M2';*/
	case 14: /*return 'Received M2D';*/
	case 15: /*return 'Send M3';*/
	case 16: /*return 'Received M3';*/
	case 17: /*return 'Send M4';*/
	case 18: /*return 'Received M4';*/
	case 19: /*return 'Send M5';*/
	case 20: /*return 'Received M5';*/
	case 21: /*return 'Send M6';*/
	case 22: /*return 'Received M6';*/
	case 23: /*return 'Send M7';*/
	case 24: /*return 'Received M7';*/
	case 25: /*return 'Send M8';*/
	case 26: /*return 'Received M8';*/
	case 27: /*return 'Processing EAP Response (ACK)';*/
	case 28: /*return 'Processing EAP Request (Done)';*/
	case 29: /*return 'Processing EAP Response (Done)';*/
	case 35: /*return 'SCAN AP';*/
	case 36: /*return 'EAPOL START SENT';*/
	case 37: /*return 'WSC_EAP_RSP_DONE_SENT';*/
	case 38: /*return 'WAIT PINCODE';*/
	case 39: /*return 'WSC_START_ASSOC';*/
		return __wps_processing+'&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<div class="loading_user"></div>';
	case 2: /*return 'WSC Fail(Ignore this if Intel/Marvell registrar used)';*/
	case 30: /*return 'Sending EAP-Fail';*/
	case 31: /*return 'WSC_ERROR_HASH_FAIL';*/
	case 32: /*return 'WSC_ERROR_HMAC_FAIL';*/
	case 33: /*return 'WSC_ERROR_DEV_PWD_AUTH_FAIL';*/
	case 0x101: /*return 'PBC:TOO MANY AP";*/
	case 0x102: /*return 'PBC:NO AP';*/
	case 0x103: /*return 'EAP_FAIL_RECEIVED';*/
	case 0x104: /*return 'EAP_NONCE_MISMATCH';*/
	case 0x105: /*return 'EAP_INVALID_DATA';*/
	case 0x106: /*return 'PASSWORD_MISMATCH';*/
	case 0x107: /*return 'EAP_REQ_WRONG_SMI';*/
	case 0x108: /*return 'EAP_REQ_WRONG_VENDOR_TYPE';*/
	case 0x109: /*return 'PBC_SESSION_OVERLAP';*/
		return __error_code+status;
	case 34:
		return __wps_configed;
	default:
		return __error_code+__wps_unknown;
	}
}

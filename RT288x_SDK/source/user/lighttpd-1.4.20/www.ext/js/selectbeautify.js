$.fn.extend({
	adjust_pos:function(obj) {
		var pos = $(obj).offset();
		var x = pos.left;
		var y = pos.top+$(obj).outerHeight()-1;

		if (this.css("position")=="fixed") {
			y -= $(document).scrollTop();
		}
		return this.css({left:x,top:y})
	},
});
$.selectBeautify=function(opt) {
	var opt = opt || {};
	var maxHeight=opt.maxHeight||"500px";
	var selects=$('body').find("select.mctselect");
	if (selects.length>0) {
		if ($("#mctselecthide").length<1) {
			$("body").append("<div id='mctselecthide' style='position:absolute; display:none'></div>")
		}
		cut_string=function(str) {
			var substr = str;
			/*if (str.length > 23) {
				substr = str.substring(0, 23);
				substr = substr + '..';
			} else 
				substr = str;*/
			return substr;
		};
		selects.each(function() {
			var select=this;
			var input=$("<div class='select-label tag'></div>");
			input.append("<div class='select-sublabel' id='inside-field'></div>");
			var inside_input = input.find("#inside-field");
			inside_input.css("width",(parseInt(this.style.width)-40)+"px");
			inside_input.css("max-width",(parseInt(this.style.width)-40)+"px");
			inside_input.css("text-overflow", "ellipsis");
			inside_input.css("overflow", "hidden");
			input.css("width",parseInt(this.style.width)+"px");
			input.css("max-width",parseInt(this.style.width)+"px");
			input.css("display",this.style.display);
			input.css("text-overflow", "ellipsis");
			input.css("overflow", "hidden");
			input.insertAfter(this);
			var code = this.options[this.selectedIndex]&&this.options[this.selectedIndex].title;
			var substr = cut_string(code);
			input.attr("title", code);
			/*input.html(substr);*/
			inside_input.attr("title", code);
			inside_input.html(substr);
			this.style.display="none";
			input.click(function() {
				var div=$("#mctselecthide").empty().attr("class",select.className);
				if ($(".panel-mask").length) {
					div.css("z-index",parseInt($(".panel-mask").css("z-index"),10)+10)
				}
				$(select).hasClass("extend")?div.css("width",""):div.css("width",$(this).innerWidth());
				for(var i=0;i<select.options.length;i++) {
					var item=select.options[i];
					var a=$("<a href='javascript:void(0);' class='nowrap'></a>").css("color",item.style.color).addClass(item.className).html("<span>"+item.text+"</span>").appendTo(div);
					/*  add hint for each item  */
					/*  format: SSID(00:00:00:00:00:00)  */
					a.attr("title", "("+item.getAttribute("data-channel")+")"+item.title);
					if (i==select.selectedIndex) {
						a.addClass("selected")
					}
					a.click(function() {
						var n=$(this).index();
						select.selectedIndex=n;
						var code = select.options[n].text;
						var substr = cut_string(code);
						var inside_input = input.find('#inside-field');
						input.attr("title", code);
						/*input.html(substr);*/
						inside_input.attr("title", code);
						inside_input.html(substr);
						div.hide();
						$(select).change();
					})
				}
				var noscroll=(select.options.length<10||$(select).hasClass("noscroll"));
				if (/msie 6/i.test(window.navigator.userAgent)) {
					div.css("height",noscroll?"auto":maxHeight).css("overflow-y",noscroll?"hidden":"scroll")
				} else {
					div.css("max-height",noscroll?"10000px":maxHeight);
				}
				div.adjust_pos(this);
				if (window.activeDummySelect==select) {
					div.slideToggle(100);
					$(this).toggleClass("select-open")
				} else {
					div.hide().slideDown(100);
					window.activeDummySelect=select;
					$(".tag").removeClass("select-open");
					$(this).addClass("select-open");
				}
				if (!select.selectedIndex>6&&div[0].scrollHeight>div.height()) {
					div.scrollTop((select.selectedIndex-3)*div[0].firstChild.offsetHeight);
				}
			})
		});
		$(document).click(function(e) {
			/*  when we click on any other place, we should hide the list  */
			if(!$(e.target).is(".tag")&&!$(e.target).is("#mctselecthide")) {
				$("#mctselecthide").hide();
				$(".tag").removeClass("select-open");
			}
		});
	}
};
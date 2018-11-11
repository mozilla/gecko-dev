/**
 * ��������ͨ����ʾ�ʼ���,������ӵ�����163���µ�ҳ��
 */
function NtesMailInfo(){
	// ������ʾ��ͬ��������
	// ��ʾ�ʺ�
	this.showAccount = true;
	// ��ʾ�ʼ���
	this.showMsgCount = true;
	// ��ʾ�˳�|��¼
	this.showLoginout = true;
	// ��ʾ�ʺź�׺
	this.showAccountSuffix  = true;
	// ��ʾ�ʺ����ĳ���
	this.maxAccountLength = 7;

	this.getCookie = function (sName){
		var sSearch = sName + "=";
		if(document.cookie.length > 0) {
			var sOffset = document.cookie.indexOf(sSearch);
			if(sOffset != -1) {
				sOffset += sSearch.length;
				sEnd = document.cookie.indexOf(";", sOffset);
				if(sEnd == -1) sEnd = document.cookie.length;
				return unescape(document.cookie.substring(sOffset, sEnd));
			}
			else return "";
		}
	};
	// ��ʼ��
	this.init = function (){
		if(this.P_INFO){ // ���cookie��pinfo,��ȡ�ʺ�
			var arr = this.P_INFO.split("|");
			this.username = arr[0];
			if(this.username.indexOf("@126.com") > -1){
				this.domain = "126.com";
			}
			if(this.username.indexOf("@yeah.net") > -1){
				this.domain = "yeah.net";
			}
			if(this.username.indexOf("@163.com") > -1){
				this.domain = "163.com";
			}
			/*if(this.username.indexOf("@188.com") > -1){
				this.domain = "188.com";
			}
			if(this.username.indexOf("@vip.163.com") > -1){
				this.domain = "vip.163.com";
			}*/
			if(arr[2] == 1){
				this.isLogin = true;
			}
		}else{// ���򷵻�
			return;
		}
		if(this.cm_newmsg){// ��������ʼ���Ŀcookie,�����ʺź�pinfo�����һ��,����hasnew���Ϊtrue
			var arr = this.cm_newmsg.split("&");
			if(arr[0]){
				var sUserName = arr[0].substr(5);
				if(sUserName == this.username){
					this.hasnew = true;
					if(arr[1]){
						this.newCount = arr[1].substr(4);
					}
				}
			}
		}
	};
	// ����html
	this.render = function (){
		if(this.domain == "") return;
		if(this.hasnew){
			var sUserName = this.username;
			if(sUserName.indexOf("@") > -1){
				sUserName = sUserName.split("@")[0];
			}
			if(sUserName.length > this.maxAccountLength){
				sUserName = sUserName.substring(0, this.maxAccountLength) + "..";
			}
			this.$("dvNewMsg").style.display = "";		// ��ʾ����
			
			this.$("imgNewMsg").title = "����"+ this.newCount +"��δ���ʼ�";
			this.$("lnkNewMsg").innerHTML = this.newCount > 999 ? "999+" : this.newCount; // ���ʼ���Ŀ
			if(this.newCount == 0){ // �ʼ���Ϊ0���ߴ���0����ʾ��ͬ��ͼ��
				this.$("imgNoNewMsg").style.display = "";
				this.$("imgNewMsg").style.display = "none";
			}else{
				this.$("imgNoNewMsg").style.display = "none";
				this.$("imgNewMsg").style.display = "";
			}
			
			this.$("lnkNewMsg").href = this.$("lnkMsgImg").href = this.getLoginUrl(); // �����ʼ���������
		}else{
			if(this.domain != "163.com" && location.hostname.indexOf("163.com") > -1){
				location.href = this.getShowNewMsgUrl();
			}else{
				if(!window.gGetNewCount){
					window.gGetNewCount = true;
					void('<iframe src="about:blank" style="display:none;" id="ifrmNtesMailInfo" onloaddisabled="gNtesMailInfo=new NtesMailInfo();"></iframe>');
					this.$("ifrmNtesMailInfo").src = this.getNewCountUrl();
				}
			}
		}
	};
	this.redirect = function (bType){
		if(this.redirected) return;
		this.redirected = true;
		if(bType == "iframe"){
			this.$("ifrmNtesMailInfo").src = this.getShowNewMsgUrl();
		}else{
			location.href = this.getShowNewMsgUrl();
		}
	};
	this.$ = function (sId){
		return document.getElementById(sId);
	};
	this.P_INFO = this.getCookie("P_INFO");			// ��ȡpinfocookie
	this.cm_newmsg = this.getCookie("cm_newmsg");	// ��ȡ���ʼ���Ŀcookie
	this.isLogin = this.getCookie("S_INFO") ? true : false; // ��ǰ�Ƿ��¼urs
	this.username = "";								// �ʺ�
	this.hasnew = false;							// cookie�Ƿ������ʼ���Ŀ
	this.domain = "";						// ����
	this.newCount = 0;								// ���ʼ���Ŀ
	this.redirected = false;						// �Ƿ�redirect
	this.isHomePage = location.hostname == "www.163.com" ? true : false;
	// ����,show:��ʾ��Ŀҳ��, crossdomain:������תҳ��, init:����js��163Ƶ��ҳ��
	this.type = (location.href.indexOf("/mailinfo/shownewmsg_0225.htm") > -1 ? "show" : (location.href.indexOf("/mailinfo/crossdomain_0225.htm") > -1 ? "crossdomain" : "init")); 
	
	this.getShowNewMsgUrl = function (){// ��ʾ�ʼ���Ŀ��Ϣҳ��
		return "httpdisabled://p.mail."+ this.domain +"/mailinfo/shownewmsg_www_1222.htm";
	};
	
	this.getNewCountUrl = function (){ // ��ȡ���ʼ��ӿ�url
		return "httpdisabled://msg.mail."+ this.domain +"/cgi/mc?funcid=getusrnewmsgcnt&fid=1&addSubFdrs=1&language=0&style=0&template=newmsgres_setcookie.htm&username=" + this.username;
	};
	this.getLoginUrl = function (){ // ��ȡ��¼url
		var oEntryUrl = {
			"163.com" : "httpdisabled://entry.mail.163.com/coremail/fcg/ntesdoor2?lightweight=1&verifycookie=1&language=-1&style=-1&from=newmsg_www",
			"126.com" : "httpdisabled://entry.mail.126.com/cgi/ntesdoor?lightweight=1&verifycookie=1&language=-1&style=-1&from=newmsg_www",
			"yeah.net" : "httpdisabled://entry.mail.yeah.net/cgi/ntesdoor?lightweight=1&verifycookie=1&language=-1&style=-1&from=newmsg_www"
		};
		if(!this.isLogin){
			oEntryUrl = {
				"163.com" : "httpdisabled://email.163.com/?from=newmsg#163",
				"126.com" : "httpdisabled://email.163.com/?from=newmsg#126",
				"yeah.net" : "httpdisabled://email.163.com/?from=newmsg#yeah"
			}
		}
		return oEntryUrl[this.domain];
	};
	
	// if(!this.isLogin) return; // ���û�е�¼ֱ�ӷ���
	this.init(); // ��ʼ��
	this.render();// ����html
}
var gNtesMailInfo = new NtesMailInfo();
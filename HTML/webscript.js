<!-- Version 1 18/10/2015 -->
var Main
var fromleft;

function initialize()
{	
	var w=window,d=document,e=d.documentElement,g=d.getElementsByTagName('body')[0];
	x=w.innerWidth||e.clientWidth||g.clientWidth;	y=w.innerHeight||e.clientHeight||g.clientHeight; 
	Main = document.getElementById("main");
	w = x;	
	if (w > 920) {w = 920;}
 	fromleft = (x / 2) - (x - 150)/2;
	if (fromleft < 0) {fromleft = 0;}
	Main.style.left = fromleft + "px";
	Main.style.width = x - 150 + "px";

}
function newmsg(Key)
{
var param = "toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=yes,resizable=no,titlebar=no,toobar=no,left=100,top100,width=800,height=700";
window.open("NewMsg?" + Key,"_blank",param);
}
function Reply(Num, Key)
{
var param = "toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=yes,resizable=no,titlebar=no,toobar=no,left=100,top100,width=800,height=700";
window.open("Reply/" + Num + "?" + Key,"_blank",param);
}
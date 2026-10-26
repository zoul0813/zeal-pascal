program FormattedOutput;
var s:string[8];r:real;i:integer;b:boolean;c:char;scratch:string[30];
function width:integer;
begin s:='new';r:=99;i:=99;b:=true;c:='Z';width:=8 end;
function places:integer;
begin r:=88;places:=2 end;
function nestedwidth:integer;
begin write('INNER;');str(3.5:1:1,scratch);nestedwidth:=8 end;
begin
 writeln('DEFAULT[',0,',',(-32767-1),',',32767,',',true,',',false,',','X',',','text',']');
 writeln('WIDTH[',-123:7,',',12:1,',',true:7,',',false:2,',','X':3,',','text':6,']');
 writeln('SIGNED[',7:0,',',7:-1,',',7:(-32767-1),']');
 writeln('REALDEFAULT[',1.25,',',-1.25,',',0.0,']');
 writeln('SCIENTIFIC[',1.25:1,',',1.25:9,',',-1.25:9,']');
 writeln('FIXED[',1.25:8:2,',',1.25:1:0,',',-0.0001:1:2,',',0.25:1:1,']');
 writeln('ROUND[',0.15:1:1,',',2.675:1:2,',',9.99999999999:1:1,']');
 writeln('PRECISION[',1.25:1:24,',',1.25:1:25,',',1.25:1:-1,']');
 s:='old';writeln('STRING-VALUE[',s:width,']');
 r:=1.25;writeln('REAL-VALUE[',r:width:places,']');
 i:=7;writeln('INTEGER-VALUE[',i:width,']');
 b:=false;writeln('BOOLEAN-VALUE[',b:width,']');
 c:='A';writeln('CHAR-VALUE[',c:width,']');
 writeln('OUTER[',1.25:nestedwidth:2,'];',scratch);
 writeln('RAW[',#0:3,#255:2,']');
 write('EMPTY[');write('':5);writeln(']');
 writeln('CLAMP-INT[',7:300,']');
 writeln('CLAMP-REAL[',1.25:32767,']');
 writeln('FORMAT-OUTPUT-DONE');
end.

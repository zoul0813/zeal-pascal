program Loan;
{$R+}
type labeltext=string[40];
var principal,annual,years,rate,payment,balance,interest,paid,total,discount,factor:real;
    months,month:integer;
procedure ask(prompt:labeltext;fallback,low,high:real;var result:real);
var s:string[40];value:real;code:integer;ok:boolean;
begin
  repeat
    write(prompt,' [',fallback:0:2,']> ');
    if eof(Input)then halt;readln(Input,s);value:=fallback;code:=0;
    if s<>''then val(s,value,code);
    ok:=(code=0)and(value>=low)and(value<=high);
    if ok then result:=value else writeln('Enter a value from ',low:0:2,' to ',high:0:2,'.')
  until ok
end;
{ Avoid Integer cents: even a modest loan exceeds 32767 cents. }
function cents(value:real):real;
begin cents:=int(value*100.0+0.5)/100.0 end;
begin
  writeln('Loan - a monthly repayment table.');
  ask('Principal',10000.0,1.0,1000000.0,principal);
  ask('Annual rate %',6.0,0.0,25.0,annual);
  ask('Years (whole number)',3.0,1.0,30.0,years);
  if years<>int(years)then begin writeln('Use whole years. Run again.');halt end;
  principal:=cents(principal);months:=trunc(years)*12;rate:=annual/1200.0;
  if rate=0.0 then payment:=principal/months
  else if rate<0.0001 then begin
    { Sum discounted payments to avoid cancellation for very small rates. }
    factor:=1.0;discount:=0.0;
    for month:=1 to months do begin
      factor:=factor/(1.0+rate);discount:=discount+factor
    end;
    payment:=principal/discount
  end else payment:=principal*rate/(1.0-exp(-months*ln(1.0+rate)));
  payment:=cents(payment);
  writeln('Monthly payment: ',payment:0:2);
  writeln('Month    Payment   Interest     Balance');
  balance:=principal;total:=0.0;month:=0;
  while(month<months)and(balance>0.0)do begin
    month:=month+1;interest:=cents(balance*rate);paid:=payment;
    if(month=months)or(paid>balance+interest)then paid:=balance+interest;
    balance:=cents(balance+interest-paid);total:=total+interest;
    writeln(month:5,' ',paid:10:2,' ',interest:10:2,' ',balance:11:2)
  end;
  writeln('Total interest: ',total:0:2);
  writeln('Final balance: ',balance:0:2);
  writeln('Illustration only; lender rounding and fees may differ.')
end.

{
  LOAN - Monthly repayment and amortization table.

  Uses Real arithmetic and explicit cent rounding rather than storing
  cents in a 16-bit Integer. This is an illustration, not a lender
  quotation; fees and rounding rules may differ.
}

program Loan;

{$R+}

type
  labeltext = string[40];

var
  principal, annual, years, rate, payment, balance, interest,
    paid, total, discount, factor: real;
  months, month: integer;

{ Read a line before conversion so invalid input can be retried. }
procedure Ask(prompt: labeltext; fallback, low, high: real; var result: real);
var
  s: string[40];
  value: real;
  code: integer;
  ok: boolean;

begin
  repeat
    Write(prompt, ' [', fallback:0:2, ']> ');
    if Eof(Input) then
      Halt;
    ReadLn(Input, s);
    value := fallback;
    code := 0;
    if s <> '' then
      Val(s, value, code);
    ok := (code = 0) and (value >= low) and (value <= high);
    if ok then
      result := value
    else
      WriteLn('Enter a value from ', low:0:2, ' to ', high:0:2, '.')
  until ok
end;

{ Avoid Integer cents: even a modest loan exceeds 32767 cents. }
function Cents(value: real): real;
begin
  Cents := Int(value * 100.0 + 0.5) / 100.0
end;

begin
  WriteLn('Loan - a monthly repayment table.');
  Ask('Principal', 10000.0, 1.0, 1000000.0, principal);
  Ask('Annual rate %', 6.0, 0.0, 25.0, annual);
  Ask('Years (whole number)', 3.0, 1.0, 30.0, years);
  if years <> Int(years) then
  begin
    WriteLn('Use whole years. Run again.');
    Halt
  end;
  principal := Cents(principal);
  months := Trunc(years) * 12;
  rate := annual / 1200.0;

  { Avoid division by zero in the ordinary annuity formula. }
  if rate = 0.0 then
    payment := principal / months
  else if rate < 0.0001 then
  begin
    { Sum discounted payments to avoid cancellation for very small rates. }
    factor := 1.0;
    discount := 0.0;
    for month := 1 to months do
    begin
      factor := factor / (1.0 + rate);
      discount := discount + factor
    end;
    payment := principal / discount
  end
  else
    payment := principal * rate / (1.0 - Exp(-months * Ln(1.0 + rate)));
  payment := Cents(payment);
  WriteLn('Monthly payment: ', payment:0:2);
  WriteLn('Month    Payment   Interest     Balance');
  balance := principal;
  total := 0.0;
  month := 0;
  while (month < months) and (balance > 0.0) do
  begin
    month := month + 1;
    interest := Cents(balance * rate);
    paid := payment;

    { The last instalment clears the balance, absorbing cent rounding;
      an earlier payoff is capped at the amount owed. }
    if (month = months) or (paid > balance + interest) then
      paid := balance + interest;
    balance := Cents(balance + interest - paid);
    total := total + interest;
    WriteLn(month:5, ' ', paid:10:2, ' ', interest:10:2, ' ', balance:11:2)
  end;
  WriteLn('Total interest: ', total:0:2);
  WriteLn('Final balance: ', balance:0:2);
  WriteLn('Illustration only; lender rounding and fees may differ.')
end.

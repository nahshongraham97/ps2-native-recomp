# Corrections during0406 source recovery

Earlier WORK.md said "`28990 also supplies a live setting`".
That field interpretation was premature. Actual Xbox289CE pushes
`0x3f800000` immediately before289D3 calls2BCD60:it supplies constant1.0.
The saved full original listing is `source-xbox-05/00028990.asm`.
WORK.md is corrected in place;no runtime implementation/test was based on
the incorrect interpretation.

The distinct caller2BFEB0 still passes the actual835350 word at2BFF06..
2BFF0D after constructing the Core module prefix. That word's producers
and supported numeric domain remain OPEN. A source caller alone does not
prove an adjustable UI setting or a nondefault value.

Candidate PS2 1A84A0 is a raster callback/flag adapter,not image creation.
It is retained as negative discovery,not selected as a matching implementation.
The bounded allocator-tag search instead points to1A81C8 inside the next
actual image-creation candidate. Instructions/ABI must settle its meaning.

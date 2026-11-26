struct kernel_in{std::uint64_t r2;std::uint64_t r4;std::uint64_t r31;};
struct kernel_out{std::uint64_t r2;std::int64_t budget;};
kernel_out kernel(mka::stage_a::GuestMemory&memory,const kernel_in&input,std::int64_t entry_budget){
if(entry_budget<5)throw mka::stage_a::MemoryRefusal("N3 unproved scheduling/input fuel");
std::uint64_t L_0016AE10_r2{};
std::uint64_t L_0016AE10_r4{};
std::uint64_t L_0016AE10_r31{};
std::uint64_t L_0016AE10_l0{};
std::int64_t L_0016AE10_fuel{};
std::uint64_t L_0016AE14_r2{};
std::uint64_t L_0016AE14_r4{};
std::uint64_t L_0016AE14_r31{};
std::uint64_t L_0016AE14_l0{};
std::int64_t L_0016AE14_fuel{};
std::uint64_t L_0016AE18_r2{};
std::uint64_t L_0016AE18_r4{};
std::uint64_t L_0016AE18_r31{};
std::uint64_t L_0016AE18_l0{};
std::int64_t L_0016AE18_fuel{};
std::uint64_t L_0016AE1C_r2{};
std::uint64_t L_0016AE1C_r4{};
std::uint64_t L_0016AE1C_r31{};
std::uint64_t L_0016AE1C_l0{};
std::int64_t L_0016AE1C_fuel{};
L_0016AE10_r2=input.r2;
L_0016AE10_r4=input.r4;
L_0016AE10_r31=input.r31;
L_0016AE10_fuel=entry_budget;goto L_0016AE10;
L_0016AE10:{
const std::uint8_t value_1=memory.load<std::uint8_t>(static_cast<std::uint32_t>((static_cast<std::uint32_t>(L_0016AE10_r4) + std::uint32_t{20})));
const std::uint64_t value_2=static_cast<std::uint64_t>(static_cast<std::uint64_t>( value_1));
const std::int64_t value_3=L_0016AE10_fuel-1;
const std::uint64_t value_4=value_2;
const std::uint64_t value_5=L_0016AE10_r4;
const std::uint64_t value_6=L_0016AE10_r31;
const std::uint64_t value_7=L_0016AE10_l0;
const std::int64_t value_8=value_3;
L_0016AE14_r2=value_4;
L_0016AE14_r4=value_5;
L_0016AE14_r31=value_6;
L_0016AE14_l0=value_7;
L_0016AE14_fuel=value_8;goto L_0016AE14;
}
L_0016AE14:{
const std::uint64_t value_9=static_cast<std::uint64_t>((static_cast<std::uint64_t>(L_0016AE14_r2) << std::int32_t{58}));
const std::int64_t value_10=L_0016AE14_fuel-1;
const std::uint64_t value_11=value_9;
const std::uint64_t value_12=L_0016AE14_r4;
const std::uint64_t value_13=L_0016AE14_r31;
const std::uint64_t value_14=L_0016AE14_l0;
const std::int64_t value_15=value_10;
L_0016AE18_r2=value_11;
L_0016AE18_r4=value_12;
L_0016AE18_r31=value_13;
L_0016AE18_l0=value_14;
L_0016AE18_fuel=value_15;goto L_0016AE18;
}
L_0016AE18:{
const std::uint64_t value_16=static_cast<std::uint64_t>((static_cast<std::uint64_t>(L_0016AE18_r2) >> std::int32_t{63}));
const std::int64_t value_17=L_0016AE18_fuel-1;
const std::uint64_t value_18=value_16;
const std::uint64_t value_19=L_0016AE18_r4;
const std::uint64_t value_20=L_0016AE18_r31;
const std::uint64_t value_21=L_0016AE18_l0;
const std::int64_t value_22=value_17;
L_0016AE1C_r2=value_18;
L_0016AE1C_r4=value_19;
L_0016AE1C_r31=value_20;
L_0016AE1C_l0=value_21;
L_0016AE1C_fuel=value_22;goto L_0016AE1C;
}
L_0016AE1C:{
const std::int64_t value_23=L_0016AE1C_fuel-2;
const std::uint64_t value_24=static_cast<std::uint64_t>(static_cast<std::uint32_t>(L_0016AE1C_r31));
const std::uint64_t value_25=static_cast<std::uint64_t>(((std::uint64_t{0} < static_cast<std::uint64_t>(L_0016AE1C_r2)) ? std::uint64_t{1} : std::uint64_t{0}));
if(static_cast<bool>( (static_cast<std::uint32_t>(value_24) & std::uint32_t{3})))throw mka::stage_a::MemoryRefusal("N2 invalid ordinary return address");
return {value_25,value_23};
}
}

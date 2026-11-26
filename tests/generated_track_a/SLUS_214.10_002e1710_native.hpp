struct kernel_in{std::uint64_t r31;};
struct kernel_out{std::int64_t budget;};
kernel_out kernel(mka::stage_a::GuestMemory&memory,const kernel_in&input,std::int64_t entry_budget){
if(entry_budget<2)throw mka::stage_a::MemoryRefusal("N3 unproved scheduling/input fuel");
std::uint64_t L_002E1710_r31{};
std::uint64_t L_002E1710_l0{};
std::int64_t L_002E1710_fuel{};
L_002E1710_r31=input.r31;
L_002E1710_fuel=entry_budget;goto L_002E1710;
L_002E1710:{
const std::int64_t value_1=L_002E1710_fuel-2;
const std::uint64_t value_2=static_cast<std::uint64_t>(static_cast<std::uint32_t>(L_002E1710_r31));
if(static_cast<bool>( (static_cast<std::uint32_t>(value_2) & std::uint32_t{3})))throw mka::stage_a::MemoryRefusal("N2 invalid ordinary return address");
return {value_1};
}
}

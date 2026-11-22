#pragma once
namespace mka::retail {
// Native mathematical values only. Source matrix flags/padding and owning
// matrix services are separate; these are not an extension byte layout.
struct RetailRendererPoint3 {float x{},y{},z{};bool operator==(const RetailRendererPoint3&)const=default;};
struct RetailRendererAffine {RetailRendererPoint3 right,up,at,translation;bool operator==(const RetailRendererAffine&)const=default;};
}

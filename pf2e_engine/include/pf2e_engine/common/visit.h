#pragma once

template<class... Ts>
struct VisitorHelper : Ts... { using Ts::operator()...; };

#pragma once

namespace math_editor_examples {
    inline constexpr char longChain[] = R"(0<|a_n-\frac{1+\sqrt{5}}{2}|=\left|\frac{1+2a_{n-1}-\frac{1+\sqrt{5}}{2}(1+a_{n-1})}{1+a_{n-1}}\right|=\frac{1}{|1+a_{n-1}|}\cdot \left|\frac{3-\sqrt{5}}{2}a_{n-1}+\frac{1-\sqrt{5}}{2}\right|=\frac{3-\sqrt{5}}{2|1+a_{n-1}|}\left|a_{n-1}-\frac{1+\sqrt{5}}{2}\right|<\frac{1}{4}|a_{n-1}-\frac{1+\sqrt{5}}{2}|<...<\frac{1}{4^n}|a_0-\frac{1+\sqrt{5}}{2}|\rightarrow0(n\rightarrow \infty))";
}

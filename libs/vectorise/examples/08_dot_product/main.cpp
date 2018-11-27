//------------------------------------------------------------------------------
//
//   Copyright 2018 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include <iostream>

using array_type  = fetch::memory::SharedArray<type>;
using vector_type = typename array_type::vector_register_type;

template <typename D>
using _S = fetch::memory::SharedArray<D>;

template <typename D>
using _M = Matrix<D, _S<D>>;

void Dot(array_type const &mA, array_type const &mB, array_type &mC)
{
  for (std::size_t i = 0; i < mA.height(); ++i)
  {
    auto A = mA.slice(i * mA.padded_width(), mA.padded_height());
    for (std::size_t j = 0; j < mB.height(); ++j)
    {

      auto B   = mB.slice(j * mB.padded_width(), mA.padded_height());
      type ele = A.in_parallel()
                     .SumReduce([](vector_type const &a, vector_type const &b) { return a * b; }, B)
                         mC.Set(i, j, ele);
    }
  }
}

int main()
{
  _M<double> A, B, C, R;
  R.Resize(7, 7);
  A = _M<double>(R"(
-0.110255536702 1.16455707117 0.483156207079 -0.822864927503 2.1399957063 0.502458535614 0.0104965839893 ;
0.248408187151 -0.069653723501 0.189842928665 0.276933298712 0.921931208942 -1.31026029885 -1.12882684957 ;
0.121318976781 -0.97751914839 -0.717617609338 -0.924317559786 0.233683935993 0.402642247873 -0.191914722504 ;
-1.01644131688 1.27118620892 0.289336475212 0.84329369777 -0.592601127047 -1.38145681252 1.01862989384 ;
-0.519844550177 -0.643278547153 -0.946077690659 1.08010014881 0.725239505415 1.71566775596 1.16751986319 ;
1.70134433962 -0.176658772545 -0.969849860334 0.925512715904 -0.872133907354 1.92808342061 -1.09108310394 ;
0.419388743269 -1.11579661665 -0.144438994263 -0.993875730161 -0.927831594645 0.0835181806493 -0.856972684823
)");
  B = _M<double>(R"(
-0.599018869905 1.16794149153 0.73479242779 -0.695886341907 0.268931568945 0.289327695646 1.09872219467 ;
0.730496158961 -1.45277115851 -0.50189312604 0.0253094043775 0.623443070562 -0.467176654852 -0.911519033508 ;
0.527662784095 -0.702879116321 -0.621857012945 1.30079513141 0.0107413745168 -0.0675452169503 0.523792754301 ;
0.965677623804 -0.101697262564 1.19339410195 -0.219028177365 -1.35588086777 0.113968539915 -1.6504006069 ;
-1.46713852363 0.964046301291 0.535025851846 -1.0392953995 0.0400928482028 1.48345835868 -0.477589421502 ;
-0.894957100226 -0.0878673943496 0.731317325442 1.11820558017 -0.376684651897 -0.403513143519 -1.46206701665 ;
-1.36333317473 -0.647806195524 -0.566366799723 1.92974334085 0.140776091852 0.716927530166 -0.251861064705
)");
  C = _M<double>(R"(
-3.22658840046 -0.0644187677103 -0.441489607854 -0.727062626009 1.71528818806 2.27700172248 -1.3308366894 ;
1.52691142649 1.9648917181 0.604293611972 -4.58998153493 0.0215362451946 1.21021742862 1.73850105514 ;
-2.49954976554 2.47443064428 0.451115151386 -1.00316253737 0.499438153916 0.481503581171 1.52197543972 ;
3.22152671479 -4.43280054604 -2.46267557141 1.96799215523 0.018874402975 -0.40276678857 -1.46946787996 ;
-3.80589167465 0.674610130336 2.79966438596 2.29599096283 -2.46832937552 1.55771500264 -5.41179143513 ;
0.275306422548 2.52789777908 4.6077879845 -1.69581471346 -1.83273816972 -2.10824390787 -2.13279835141 ;
0.352566029247 1.96675732418 -0.17806542795 -0.886341525336 0.573876124578 -1.48538702158 3.57935001377
)");

  Dot(A, B, R);
  std::cout << R.AllClose(C)) << std::endl;
  return 0;
}
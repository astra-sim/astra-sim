/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/

#include "Torus3D.hh"
namespace AstraSim{
    Torus3D::Torus3D(int id, int total_nodes, int local_dim, int vertical_dim) {
        int horizontal_dim=total_nodes/(vertical_dim*local_dim);
        local_dimension=new RingTopology(RingTopology::Dimension::Local,id,local_dim,id%local_dim,1);
        vertical_dimension=new RingTopology(RingTopology::Dimension::Vertical,id,vertical_dim,id/(local_dim*horizontal_dim),local_dim*horizontal_dim);
        horizontal_dimension=new RingTopology(RingTopology::Dimension::Horizontal,id,horizontal_dim,(id/local_dim)%horizontal_dim,local_dim);
    }
    Torus3D::~Torus3D(){
        delete local_dimension;
        delete vertical_dimension;
        delete horizontal_dimension;
    };
}
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

#include "QueueLevels.hh"
namespace AstraSim {
QueueLevels::QueueLevels(int total_levels, int queues_per_level, int offset) {
  int start = offset;
  // levels.resize(total_levels);
  for (int i = 0; i < total_levels; i++) {
    QueueLevelHandler tmp(i, start, start + queues_per_level - 1);
    levels.push_back(tmp);
    start += queues_per_level;
  }
}
QueueLevels::QueueLevels(std::vector<int> lv, int offset) {
  int start = offset;
  // levels.resize(total_levels);
  int l = 0;
  for (auto &i : lv) {
    QueueLevelHandler tmp(l++, start, start + i - 1);
    levels.push_back(tmp);
    start += i;
  }
}
std::pair<int, RingTopology::Direction>
QueueLevels::get_next_queue_at_level(int level) {
  return levels[level].get_next_queue_id();
}
std::pair<int, RingTopology::Direction>
QueueLevels::get_next_queue_at_level_first(int level) {
  return levels[level].get_next_queue_id_first();
}
std::pair<int, RingTopology::Direction>
QueueLevels::get_next_queue_at_level_last(int level) {
  return levels[level].get_next_queue_id_last();
}
} // namespace AstraSim
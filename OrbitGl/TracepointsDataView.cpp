// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "TracepointsDataView.h"

#include "App.h"
#include "Localization.h"

namespace {
static const std::string kMenuActionSelect = "Hook";
static const std::string kMenuActionUnselect = "Unhook";
}  // namespace

TracepointsDataView::TracepointsDataView() : DataView(DataViewType::kTracepoints) {}

const std::vector<DataView::Column>& TracepointsDataView::GetColumns() {
  static const std::vector<Column>& columns = [] {
    std::vector<Column> columns;
    columns.resize(kNumColumns);
    columns[kColumnSelected] = {QT_TRANSLATE_NOOP("OrbitTableView", "Selected"), .0f,
                                SortingOrder::kDescending};
    columns[kColumnCategory] = {QT_TRANSLATE_NOOP("OrbitTableView", "Category"), .5f,
                                SortingOrder::kAscending};
    columns[kColumnName] = {QT_TRANSLATE_NOOP("OrbitTableView", "Name"), .2f,
                            SortingOrder::kAscending};
    return columns;
  }();
  return columns;
}

std::string TracepointsDataView::GetValue(int row, int col) {
  const TracepointInfo& tracepoint = GetTracepoint(row);

  switch (col) {
    case kColumnSelected:
      return GOrbitApp->IsTracepointSelected(tracepoint) ? "X" : "-";
    case kColumnName:
      return tracepoint.name();
    case kColumnCategory:
      return tracepoint.category();
    default:
      return "";
  }
}

#define ORBIT_PROC_SORT(Member)                                                            \
  [&](int a, int b) {                                                                      \
    return OrbitUtils::Compare(tracepoints_[a].Member, tracepoints_[b].Member, ascending); \
  }

void TracepointsDataView::DoSort() {
  bool ascending = sorting_orders_[sorting_column_] == SortingOrder::kAscending;
  std::function<bool(int a, int b)> sorter = nullptr;

  switch (sorting_column_) {
    case kColumnName:
      sorter = ORBIT_PROC_SORT(name());
      break;
    case kColumnCategory:
      sorter = ORBIT_PROC_SORT(category());
      break;
    default:
      break;
  }

  if (sorter) {
    std::stable_sort(indices_.begin(), indices_.end(), sorter);
  }
}

void TracepointsDataView::DoFilter() {
  std::vector<uint32_t> indices;
  std::vector<std::string> tokens = absl::StrSplit(ToLower(filter_), ' ');

  for (size_t i = 0; i < tracepoints_.size(); ++i) {
    const TracepointInfo& tracepoint = tracepoints_[i];
    const std::string tracepoint_string = tracepoint.name();

    bool match = true;

    for (std::string& filter_token : tokens) {
      if (tracepoint_string.find(filter_token) == std::string::npos) {
        match = false;
        break;
      }
    }

    if (match) {
      indices.emplace_back(i);
    }
  }

  indices_ = indices;
}

std::vector<std::string> TracepointsDataView::GetContextMenu(
    int clicked_index, const std::vector<int>& selected_indices) {
  std::vector<std::string> menu;

  bool enable_select = false;
  bool enable_unselect = false;
  for (int index : selected_indices) {
    const TracepointInfo& tracepoint = GetTracepoint(index);
    enable_select |= !GOrbitApp->IsTracepointSelected(tracepoint);
    enable_unselect |= GOrbitApp->IsTracepointSelected(tracepoint);
  }

  if (enable_select) menu.emplace_back(kMenuActionSelect);
  if (enable_unselect) menu.emplace_back(kMenuActionUnselect);

  Append(menu, DataView::GetContextMenu(clicked_index, selected_indices));
  return menu;
}

void TracepointsDataView::OnContextMenu(const std::string& action, int menu_index,
                                        const std::vector<int>& item_indices) {
  if (action == kMenuActionSelect) {
    for (int i : item_indices) {
      GOrbitApp->SelectTracepoint(GetTracepoint(i));
    }
  } else if (action == kMenuActionUnselect) {
    for (int i : item_indices) {
      GOrbitApp->DeselectTracepoint(GetTracepoint(i));
    }
  } else {
    DataView::OnContextMenu(action, menu_index, item_indices);
  }
}

void TracepointsDataView::SetTracepoints(const std::vector<TracepointInfo>& tracepoints) {
  tracepoints_.assign(tracepoints.cbegin(), tracepoints.cend());

  indices_.resize(tracepoints_.size());
  for (size_t i = 0; i < indices_.size(); ++i) {
    indices_[i] = i;
  }
}

const TracepointInfo& TracepointsDataView::GetTracepoint(uint32_t row) const {
  return tracepoints_.at(indices_[row]);
}

// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SamplingReportDataView.h"

#include <set>

#include "App.h"
#include "CallStackDataView.h"
#include "Localization.h"
#include "OrbitClientData/FunctionUtils.h"
#include "OrbitClientData/ModuleData.h"
#include "Path.h"
#include "absl/strings/str_format.h"

using orbit_client_protos::FunctionInfo;

SamplingReportDataView::SamplingReportDataView()
    : DataView(DataViewType::kSampling), callstack_data_view_(nullptr) {}

const std::vector<DataView::Column>& SamplingReportDataView::GetColumns() {
  static const std::vector<Column> columns = [] {
    std::vector<Column> columns;
    columns.resize(kNumColumns);
    columns[kColumnSelected] = {QT_TRANSLATE_NOOP("OrbitTableView", "Hooked"), .0f,
                                SortingOrder::kDescending};
    columns[kColumnFunctionName] = {QT_TRANSLATE_NOOP("OrbitTableView", "Name"), .5f,
                                    SortingOrder::kAscending};
    columns[kColumnExclusive] = {QT_TRANSLATE_NOOP("OrbitTableView", "Exclusive"), .0f,
                                 SortingOrder::kDescending};
    columns[kColumnInclusive] = {QT_TRANSLATE_NOOP("OrbitTableView", "Inclusive"), .0f,
                                 SortingOrder::kDescending};
    columns[kColumnModuleName] = {QT_TRANSLATE_NOOP("OrbitTableView", "Module"), .0f,
                                  SortingOrder::kAscending};
    columns[kColumnFile] = {QT_TRANSLATE_NOOP("OrbitTableView", "File"), .0f,
                            SortingOrder::kAscending};
    columns[kColumnLine] = {QT_TRANSLATE_NOOP("OrbitTableView", "Line"), .0f,
                            SortingOrder::kAscending};
    columns[kColumnAddress] = {QT_TRANSLATE_NOOP("OrbitTableView", "Address"), .0f,
                               SortingOrder::kAscending};
    return columns;
  }();
  return columns;
}

std::string SamplingReportDataView::GetValue(int row, int column) {
  const SampledFunction& func = GetSampledFunction(row);

  switch (column) {
    case kColumnSelected:
      return GOrbitApp->IsFunctionSelected(func) ? FunctionsDataView::kSelectedFunctionString
                                                 : FunctionsDataView::kUnselectedFunctionString;
    case kColumnFunctionName:
      return func.name;
    case kColumnExclusive:
      return absl::StrFormat("%.2f", func.exclusive);
    case kColumnInclusive:
      return absl::StrFormat("%.2f", func.inclusive);
    case kColumnModuleName:
      // LOG("%s", func.module_path);
      // LOG("%s", Path::GetFileName(func.module_path));
      return Path::GetFileName(func.module_path);
    case kColumnFile:
      return func.file;
    case kColumnLine:
      return func.line > 0 ? absl::StrFormat("%d", func.line) : "";
    case kColumnAddress:
      return absl::StrFormat("%#llx", func.absolute_address);
    default:
      return "";
  }
}

#define ORBIT_PROC_SORT(Member)                                                      \
  [&](int a, int b) {                                                                \
    return OrbitUtils::Compare(functions[a].Member, functions[b].Member, ascending); \
  }

#define ORBIT_CUSTOM_FUNC_SORT(Func)                                               \
  [&](int a, int b) {                                                              \
    return OrbitUtils::Compare(Func(functions[a]), Func(functions[b]), ascending); \
  }

#define ORBIT_MODULE_NAME_FUNC_SORT                                                     \
  [&](int a, int b) {                                                                   \
    return OrbitUtils::Compare(Path::GetFileName(functions[a].module_path),             \
                               Path::GetFileName(functions[b].module_path), ascending); \
  }

void SamplingReportDataView::DoSort() {
  bool ascending = sorting_orders_[sorting_column_] == SortingOrder::kAscending;
  std::function<bool(int a, int b)> sorter = nullptr;

  std::vector<SampledFunction>& functions = functions_;

  switch (sorting_column_) {
    case kColumnSelected:
      sorter = ORBIT_CUSTOM_FUNC_SORT(GOrbitApp->IsFunctionSelected);
      break;
    case kColumnFunctionName:
      sorter = ORBIT_PROC_SORT(name);
      break;
    case kColumnExclusive:
      sorter = ORBIT_PROC_SORT(exclusive);
      break;
    case kColumnInclusive:
      sorter = ORBIT_PROC_SORT(inclusive);
      break;
    case kColumnModuleName:
      sorter = ORBIT_MODULE_NAME_FUNC_SORT;
      break;
    case kColumnFile:
      sorter = ORBIT_PROC_SORT(file);
      break;
    case kColumnLine:
      sorter = ORBIT_PROC_SORT(line);
      break;
    case kColumnAddress:
      sorter = ORBIT_PROC_SORT(absolute_address);
      break;
    default:
      break;
  }

  if (!sorter) {
    return;
  }

  const auto fallback_sorter = [&](const auto& ind_left, const auto& ind_right) {
    // `SampledFunction::address` is the absolute function address. Hence it is unique and qualifies
    // for total ordering.
    return functions[ind_left].absolute_address < functions[ind_right].absolute_address;
  };

  const auto combined_sorter = [&](const auto& ind_left, const auto& ind_right) {
    if (sorter(ind_left, ind_right)) {
      return true;
    }

    if (sorter(ind_right, ind_left)) {
      return false;
    }

    return fallback_sorter(ind_left, ind_right);
  };

  std::sort(indices_.begin(), indices_.end(), combined_sorter);
}

absl::flat_hash_set<const FunctionInfo*> SamplingReportDataView::GetFunctionsFromIndices(
    const std::vector<int>& indices) {
  absl::flat_hash_set<const FunctionInfo*> functions_set;
  const CaptureData& capture_data = GOrbitApp->GetCaptureData();
  for (int index : indices) {
    SampledFunction& sampled_function = GetSampledFunction(index);
    if (sampled_function.function == nullptr) {
      const FunctionInfo* func =
          capture_data.FindFunctionByAddress(sampled_function.absolute_address, false);
      sampled_function.function = func;
    }

    const FunctionInfo* function = sampled_function.function;
    if (function != nullptr) {
      functions_set.insert(function);
    }
  }

  return functions_set;
}

absl::flat_hash_set<std::string> SamplingReportDataView::GetModulePathsFromIndices(
    const std::vector<int>& indices) const {
  absl::flat_hash_set<std::string> module_paths;
  const ProcessData* process = GOrbitApp->GetCaptureData().process();
  CHECK(process != nullptr);

  for (int index : indices) {
    const SampledFunction& sampled_function = GetSampledFunction(index);
    CHECK(sampled_function.absolute_address != 0);
    auto result = process->FindModuleByAddress(sampled_function.absolute_address);
    if (!result) {
      ERROR("result %s", result.error().message());
    } else {
      module_paths.insert(result.value().first);
    }
  }

  return module_paths;
}

const std::string SamplingReportDataView::kMenuActionSelect = "Hook";
const std::string SamplingReportDataView::kMenuActionUnselect = "Unhook";
const std::string SamplingReportDataView::kMenuActionLoadSymbols = "Load Symbols";
const std::string SamplingReportDataView::kMenuActionDisassembly = "Go to Disassembly";

std::vector<std::string> SamplingReportDataView::GetContextMenu(
    int clicked_index, const std::vector<int>& selected_indices) {
  bool enable_select = false;
  bool enable_unselect = false;
  bool enable_disassembly = false;

  if (GOrbitApp->IsCaptureConnected(GOrbitApp->GetCaptureData())) {
    absl::flat_hash_set<const FunctionInfo*> selected_functions =
        GetFunctionsFromIndices(selected_indices);

    enable_disassembly = !selected_functions.empty();

    for (const FunctionInfo* function : selected_functions) {
      enable_select |= !GOrbitApp->IsFunctionSelected(*function);
      enable_unselect |= GOrbitApp->IsFunctionSelected(*function);
    }
  }

  bool enable_load = false;
  for (const std::string& module_path : GetModulePathsFromIndices(selected_indices)) {
    const ModuleData* module = GOrbitApp->GetModuleByPath(module_path);
    if (!module->is_loaded()) {
      enable_load = true;
    }
  }

  std::vector<std::string> menu;
  if (enable_select) menu.emplace_back(kMenuActionSelect);
  if (enable_unselect) menu.emplace_back(kMenuActionUnselect);
  if (enable_load) menu.emplace_back(kMenuActionLoadSymbols);
  if (enable_disassembly) menu.emplace_back(kMenuActionDisassembly);
  Append(menu, DataView::GetContextMenu(clicked_index, selected_indices));
  return menu;
}

void SamplingReportDataView::OnContextMenu(const std::string& action, int menu_index,
                                           const std::vector<int>& item_indices) {
  if (action == kMenuActionSelect) {
    for (const FunctionInfo* function : GetFunctionsFromIndices(item_indices)) {
      GOrbitApp->SelectFunction(*function);
    }
  } else if (action == kMenuActionUnselect) {
    for (const FunctionInfo* function : GetFunctionsFromIndices(item_indices)) {
      GOrbitApp->DeselectFunction(*function);
      GOrbitApp->DisableFrameTrack(*function);
    }
  } else if (action == kMenuActionLoadSymbols) {
    std::vector<ModuleData*> modules_to_load;
    for (const std::string& module_path : GetModulePathsFromIndices(item_indices)) {
      ModuleData* module = GOrbitApp->GetMutableModuleByPath(module_path);
      if (!module->is_loaded()) {
        modules_to_load.push_back(module);
      }
    }
    GOrbitApp->LoadModules(modules_to_load);
  } else if (action == kMenuActionDisassembly) {
    int32_t pid = GOrbitApp->GetCaptureData().process_id();
    for (const FunctionInfo* function : GetFunctionsFromIndices(item_indices)) {
      GOrbitApp->Disassemble(pid, *function);
    }
  } else {
    DataView::OnContextMenu(action, menu_index, item_indices);
  }
}

void SamplingReportDataView::OnSelect(int index) {
  SampledFunction& func = GetSampledFunction(index);
  sampling_report_->OnSelectAddress(func.absolute_address, tid_);
}

void SamplingReportDataView::LinkDataView(DataView* data_view) {
  if (data_view->GetType() == DataViewType::kCallstack) {
    callstack_data_view_ = static_cast<CallStackDataView*>(data_view);
    sampling_report_->SetCallstackDataView(callstack_data_view_);
  }
}

void SamplingReportDataView::SetSampledFunctions(const std::vector<SampledFunction>& functions) {
  functions_ = functions;

  size_t num_functions = functions_.size();
  indices_.resize(num_functions);
  for (size_t i = 0; i < num_functions; ++i) {
    indices_[i] = i;
  }

  OnDataChanged();
}

void SamplingReportDataView::SetThreadID(ThreadID tid) {
  tid_ = tid;
  if (tid == SamplingProfiler::kAllThreadsFakeTid) {
    name_ = absl::StrFormat("%s\n(all threads)", GOrbitApp->GetCaptureData().process_name());
  } else {
    name_ = absl::StrFormat("%s\n[%d]", GOrbitApp->GetCaptureData().GetThreadName(tid_), tid_);
  }
}

void SamplingReportDataView::DoFilter() {
  std::vector<uint32_t> indices;

  std::vector<std::string> tokens = absl::StrSplit(ToLower(filter_), ' ');

  for (size_t i = 0; i < functions_.size(); ++i) {
    SampledFunction& func = functions_[i];
    std::string name = ToLower(func.name);
    std::string module_name = ToLower(Path::GetFileName(func.module_path));

    bool match = true;

    for (std::string& filter_token : tokens) {
      if (!(name.find(filter_token) != std::string::npos ||
            module_name.find(filter_token) != std::string::npos)) {
        match = false;
        break;
      }
    }

    if (match) {
      indices.push_back(i);
    }
  }

  indices_ = indices;
}

const SampledFunction& SamplingReportDataView::GetSampledFunction(unsigned int row) const {
  return functions_[indices_[row]];
}

SampledFunction& SamplingReportDataView::GetSampledFunction(unsigned int row) {
  return functions_[indices_[row]];
}

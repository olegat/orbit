// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_PROCESSES_DATA_VIEW
#define ORBIT_GL_PROCESSES_DATA_VIEW

#include "DataView.h"
#include "process.pb.h"

class ProcessesDataView final : public DataView {
 public:
  ProcessesDataView();

  void SetSelectionListener(const std::function<void(int32_t)>& selection_listener);
  const std::vector<Column>& GetColumns() override;
  int GetDefaultSortingColumn() override { return kColumnCpu; }
  std::string GetValue(int row, int column) override;
  std::string GetToolTip(int row, int column) override;
  std::string GetLabel() override;

  void OnSelect(int index) override;
  bool SelectProcess(const std::string& process_name);
  bool SelectProcess(int32_t process_id);
  void SetProcessList(const std::vector<orbit_grpc_protos::ProcessInfo>& process_list);
  int32_t GetSelectedProcessId() const;
  int32_t GetFirstProcessId() const;

 protected:
  void DoSort() override;
  void DoFilter() override;

 private:
  void UpdateProcessList();
  void SetSelectedItem();

  const orbit_grpc_protos::ProcessInfo& GetProcess(uint32_t row) const;

  std::vector<orbit_grpc_protos::ProcessInfo> process_list_;
  int32_t selected_process_id_;
  std::function<void(int32_t)> selection_listener_;

  enum ColumnIndex { kColumnPid, kColumnName, kColumnCpu, kNumColumns };
};

#endif  // ORBIT_GL_PROCESSES_DATA_VIEW

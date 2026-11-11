#pragma once

class MainWindow;

//	gap_buffer の動作確認テスト。結果は MainWindow の OutputBar に表示する
void do_test_gapBuffer(MainWindow *mw);

//	gap_buffer の性能計測。結果は MainWindow の OutputBar に表示する
void do_perf_gapBuffer(MainWindow *mw);

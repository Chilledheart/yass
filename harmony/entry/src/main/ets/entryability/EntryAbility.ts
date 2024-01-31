// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import hilog from '@ohos.hilog';
import UIAbility from '@ohos.app.ability.UIAbility';
import Want from '@ohos.app.ability.Want';
import window from '@ohos.window';
import yass from 'libyass.so';

/**
 * Lift cycle management of Ability.
 */
export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(0x0000, 'yass', '%{public}s', 'Ability onCreate');
    /* see https://ost.51cto.com/posts/23034 */
    yass.init(this.context.tempDir, this.context.preferencesDir);
  }

  onDestroy(): void {
    hilog.info(0x0000, 'yass', '%{public}s', 'Ability onDestroy');
    yass.destroy();
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(0x0000, 'yass', '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent("pages/DetailPage", (err, data) => {
      if (err.code) {
        hilog.error(0x0000, 'yass', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(0x0000, 'yass', 'Succeeded in loading the content. Data: %{public}s', JSON.stringify(data) ?? '');
    });
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(0x0000, 'yass', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(0x0000, 'yass', '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(0x0000, 'yass', '%{public}s', 'Ability onBackground');
  }
}
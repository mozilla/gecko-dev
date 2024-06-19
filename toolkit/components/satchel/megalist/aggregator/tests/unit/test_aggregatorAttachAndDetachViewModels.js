/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function test_aggregatorAttachAndDetachViewModels() {
  const incrementCounterAction = mockViewModel => mockViewModel.counter++;
  const mockViewModels = [
    {
      counter: 0,
    },
    {
      counter: 0,
    },
    {
      counter: 0,
    },
  ];

  const aggregator = new Aggregator();

  for (let mockViewModel of mockViewModels) {
    aggregator.attachViewModel(mockViewModel);
  }

  aggregator.forEachViewModel(incrementCounterAction);

  for (let mockViewModel of mockViewModels) {
    Assert.equal(mockViewModel.counter, 1, "Aggregator attached viewModel");
  }

  for (let mockViewModel of mockViewModels) {
    aggregator.detachViewModel(mockViewModel);
  }

  aggregator.forEachViewModel(incrementCounterAction);

  for (let mockViewModel of mockViewModels) {
    Assert.equal(mockViewModel.counter, 1, "Aggregator detached viewModel");
  }
});

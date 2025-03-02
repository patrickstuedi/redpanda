/**
 * Copyright 2020 Vectorized, Inc.
 *
 * Licensed as a Redpanda Enterprise file under the Redpanda Community
 * License (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * https://github.com/redpanda-data/redpanda/blob/master/licenses/rcl.md
 */

import { Coprocessor } from "../public/Coprocessor";

/**
 * Handle interface represents a coprocessor loaded into memory
 * @coprocessor: is a Coprocessor implementation instance
 * @checksum: is the the file's content checksum
 */
export interface Handle {
  coprocessor: Coprocessor;
  checksum: string;
}

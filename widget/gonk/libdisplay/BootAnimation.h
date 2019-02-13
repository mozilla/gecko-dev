/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BOOTANIMATION_H
#define BOOTANIMATION_H

namespace mozilla {

MOZ_EXPORT __attribute__ ((weak))
void StartBootAnimation();

/* Stop the boot animation if it's still running. */
MOZ_EXPORT __attribute__ ((weak))
void StopBootAnimation();

} // namespace mozilla

#endif /* BOOTANIMATION_H */

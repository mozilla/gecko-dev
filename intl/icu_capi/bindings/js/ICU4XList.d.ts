import { usize } from "./diplomat-runtime"

/**

 * A list of strings
 */
export class ICU4XList {

  /**

   * Create a new list of strings
   */
  static create(): ICU4XList;

  /**

   * Create a new list of strings with preallocated space to hold at least `capacity` elements
   */
  static create_with_capacity(capacity: usize): ICU4XList;

  /**

   * Push a string to the list

   * Ill-formed input is treated as if errors had been replaced with REPLACEMENT CHARACTERs according to the WHATWG Encoding Standard.
   */
  push(val: string): void;

  /**

   * The number of elements in this list
   */
  len(): usize;

  /**

   * Whether this list is empty
   */
  is_empty(): boolean;
}

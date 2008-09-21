<?php
/**
 * @file
 * Replacements for Drupal functions that would be missing when
 * any of the non-Drupal test scripts are executed.
 */

/**
 * Determine the default temporary directory.
 *
 * @return A string containing a temp directory.
 */
function file_directory_temp() {
  return sys_get_temp_dir();
}

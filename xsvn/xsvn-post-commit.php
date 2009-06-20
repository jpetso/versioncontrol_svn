<?php
// $Id$

/**
 * @file
 * Add SVN commits to the commit-log after a commit is made.
 *
 * Copyright 2009 by Daniel Hackney ('chrono325', http://drupal.org/user/384635)
 */

/**
 * Prints usage of this program.
 */
function xsvn_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: php $cli <config file> REPO_PATH REV_NUM\n\n");
}


/**
 * The main function of the hook.
 *
 * Expects the following arguments:
 *
 *   - $argv[1] - The path of the configuration file, xsvn-config.php.
 *   - $argv[2] - The path of the subversion repository.
 *   - $argv[3] - Revision number.
 */
function xsvn_init($argc, $argv) {
  $date = time(); // remember the time of the current commit for later
  $this_file = array_shift($argv);   // argv[0]

  if ($argc < 4) {
    xsvn_help($this_file, STDERR);
    exit(3);
  }

  $config_file = array_shift($argv); // argv[1]
  $repo        = array_shift($argv); // argv[2]
  $rev         = array_shift($argv); // argv[3]

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, t('Error: failed to load configuration file.') ."\n");
    exit(4);
  }
  include_once $config_file;
  xsvn_bootstrap($xsvn);

  $message = shell_exec("svnlook propget -r $rev --revprop $repo svn:log");

  $username    = xsvn_get_commit_author($rev, $repo);
  $item_paths  = xsvn_get_commit_files($rev, $repo);

  // Check temporary file storage.
  $tempdir = xsvn_get_temp_directory($xsvn['temp']);

  $operation_items = array();

  $operation = array(
    'type' => VERSIONCONTROL_OPERATION_COMMIT,
    'repo_id' => $xsvn['repo_id'],
    'date' => $date,
    'username' => $username,
    'message' => $message,
    'revision' => $rev,
    'labels' => array(), // TODO: Add support for branches and tags.
  );

  $operation = versioncontrol_insert_operation($operation, $operation_items);

  if (!empty($operation)) {
    fwrite(STDERR, t('Recorded as commit !id.', array(
          '!id' => versioncontrol_format_operation_revision_identifier($operation),
        )) ."\n");
  }
}



xsvn_init($argc, $argv);


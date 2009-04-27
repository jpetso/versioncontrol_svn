<?php
// $Id$
/**
 * @file
 * Subversion backend for Version Control API - Provides Subversion commit
 * information and account management as a pluggable backend.
 *
 * This file provides functionality to parse the output of 'svn log' (with
 * support from 'svn info') and transform it into Version Control API commits.
 *
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 * Copyright 2008, 2009 by Sam Boyer ("sdboyer", http://drupal.org/user/146719)
 */

require_once(drupal_get_path('module', 'versioncontrol_svn') .'/new.svnlib/svn.php');
require_once(drupal_get_path('module', 'versioncontrol_svn') .'/logparser.inc');

/**
 * Actually update the repository by fetching commits and other stuff
 * directly from the repository, invoking the svn executable.
 *
 * @return
 *   TRUE if the logs were updated, NULL if no new entries were fetched
 *   (without errors, though), or FALSE if fetching and updating the logs
 *   failed because of an error.
 */
function _versioncontrol_svn_log_update_repository(&$repository) {
  static $svnlib_mapping = NULL;
  if (!isset($svnlib_mapping)) {
    $svnlib_mapping = array(
      'actions' => array(
        VERSIONCONTROL_SVN_ACTION_ADD => VERSIONCONTROL_ACTION_ADDED,
        VERSIONCONTROL_SVN_ACTION_MODIFY => VERSIONCONTROL_ACTION_MODIFIED,
        VERSIONCONTROL_SVN_ACTION_REPLACE => VERSIONCONTROL_ACTION_MODIFIED,
        VERSIONCONTROL_SVN_ACTION_MOVE => VERSIONCONTROL_ACTION_MOVED,
        VERSIONCONTROL_SVN_ACTION_COPY => VERSIONCONTROL_ACTION_COPIED,
        VERSIONCONTROL_SVN_ACTION_DELETE_SIMPLE => VERSIONCONTROL_ACTION_DELETED,
        VERSIONCONTROL_SVN_ACTION_DELETE_UGLY => VERSIONCONTROL_ACTION_DELETED,
      ),
      'types' => array(
        'file' => VERSIONCONTROL_ITEM_FILE,
        'dir' => VERSIONCONTROL_ITEM_DIRECTORY,
      ),
    );
  }

  $repository['repo'] = svnlib_get_repository($repository['root']);
  $repo = &$repository['repo'];
  if (!empty($repository['svn_specific']['auth_username'])) {
    $repo->username($repository['svn_specific']['auth_username']);
    $repo->password($repository['svn_specific']['auth_password']);
  }

  $latest_rev = $repo->getLatestRev();
  $last_revision = $repository['svn_specific']['last_revision'];
  // Bugger it for the moment and use a reduced range with known problems
  // $last_revision = 650;
  // $latest_rev = 200;
  // Don't try to update if there's nothing to update.
  if ($last_revision == $latest_rev) {
    // Anyways, we want to remember the time when we tried that.
    $repository['svn_specific']['updated'] = time();
    db_query('UPDATE {versioncontrol_svn_repositories}
              SET updated = %d WHERE repo_id = %d',
              $repository['svn_specific']['updated'], $repository['repo_id']);

    return t('No new log entries to fetch.');
  }

  // Defaults to using --xml and the basic svnlog xml output parser.
  $revisions = $repo->svn('log')
    ->setParser(new VersioncontrolSvnLogHandler($repo, $last_revision + 1, $latest_rev))
    ->target('.')->revision($last_revision + 1, $latest_rev)->verbose()
    ->execute();

  foreach ($revisions as $revision) { // processing the oldest revision first
    // $actions = svnlib_more_log_info($revision, $repo);
    $operation = array(
      'type' => VERSIONCONTROL_OPERATION_COMMIT,
      'repository' => $repository,
      'date' => $revision['time_t'],
      'username' => $revision['author'],
      'message' => $revision['msg'],
      'revision' => (string) $revision['rev'],
      'labels' => array(), // no branch/tag emulation support yet
    );
    if (empty($operation['username'])) { // commit inserted by cvs2svn, for example
      $operation['username'] = '(no author)';
      $operation['uid'] = 0;
    }
    $operation_items = array();

    foreach ($revision['paths'] as $path => $rev_action) {
      $item = array(
        'type' => $svnlib_mapping['types'][$rev_action['current_item']['type']],
        'path' => $rev_action['current_item']['path'],
        'revision' => (string) $rev_action['current_item']['rev'],
        'action' => $svnlib_mapping['actions'][$rev_action['action']],
        'source_items' => array(),
      );
      if (isset($rev_action['source_item'])) {
        $item['source_items'][] = array(
          'type' => $svnlib_mapping['types'][$rev_action['source_item']['type']],
          'path' => $rev_action['source_item']['path'],
          'revision' => (string) $rev_action['source_item']['rev'],
        );
      }
      if (isset($rev_action['replaced_item'])) {
        $item['replaced_item'] = array(
          'type' => $svnlib_mapping['types'][$rev_action['replaced_item']['type']],
          'path' => $rev_action['replaced_item']['path'],
          'revision' => (string) $rev_action['replaced_item']['rev'],
        );
      }
      $operation_items[$item['path']] = $item;
    }

    // Now that was easy, wasn't it? :P
//    $operation = versioncontrol_insert_operation($operation, $operation_items);
//    if (isset($operation) && $revision['rev'] > $last_revision) {
//      $last_revision = $revision['rev'];
//    }
  }

  if ($last_revision > $repository['svn_specific']['last_revision']) {
    $repository['svn_specific']['last_revision'] = $last_revision;
    $repository['svn_specific']['updated'] = time();

    // Everything's done, remember the last revision that was captured.
//    db_query('UPDATE {versioncontrol_svn_repositories}
//              SET last_revision = %d, updated = %d WHERE repo_id = %d',
//              $repository['svn_specific']['last_revision'],
//              $repository['svn_specific']['updated'], $repository['repo_id']);
  }
  return t('Fetched !count new log entries.', array('!count' => count($revisions)));
}

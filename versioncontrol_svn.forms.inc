<?php
// $Id$
/**
 * @file
 * Subversion backend for Version Control API - Provides Subversion commit
 * information and account management as a pluggable backend.
 *
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 * Copyright 2007 by Adam Light ("aclight", http://drupal.org/user/86358)
 */

/**
 * Implementation of hook_form_alter(): Add elements to various
 * administrative forms that the Version Control API provides.
 */
function versioncontrol_svn_form_alter($form_id, &$form) {
  if ($form['#id'] == 'versioncontrol-repository-form' && $form['#vcs'] == 'svn') {
    versioncontrol_svn_repository_admin_form_alter($form_id, $form);
  }
}


/**
 * Add SVN specific elements to the add/edit repository form.
 */
function versioncontrol_svn_repository_admin_form_alter($form_id, &$form) {
  $repository = $form['#repository'];

  $form['versioncontrol_svn'] = array(
    '#type' => 'value',
    '#value' => TRUE,
  );
  $form['updated'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? $repository['svn_specific']['updated'] : 0,
  );
  $form['last_revision'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? $repository['svn_specific']['last_revision'] : 0,
  );

  $form['repository_information']['root']['#description'] = t(
    'The URL of this repository. Example: file:///svnroot/repo'
  );
  $form['repository_information']['update_method'] = array(
    '#type' => 'radios',
    '#title' => t('Update method'),
    '#description' => t('Automatic log retrieval requires cron.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['update_method']
                        : VERSIONCONTROL_SVN_UPDATE_CRON,
    '#weight' => 10,
    '#options' => array(
      VERSIONCONTROL_SVN_UPDATE_CRON => t('Automatic log retrieval.'),
      //VERSIONCONTROL_SVN_UPDATE_XSVN => t('Use external script to insert data.'),
    ),
  );
}

/**
 * Implementation of hook_versioncontrol_extract_repository_data():
 * Extract SVN specific repository additions from the repository
 * editing/adding form's submitted values.
 */
function versioncontrol_svn_versioncontrol_extract_repository_data($form_values) {
  if (!isset($form_values['versioncontrol_svn'])) {
    return array();
  }
  return array(
    'svn_specific' => array(
      'update_method' => $form_values['update_method'],
      'updated'       => $form_values['updated'],
      'last_revision' => $form_values['last_revision'],
    ),
  );
}

/**
 * Implementation of hook_versioncontrol_alter_repository_list():
 * Add SVN specific columns into the list of Subversion repositories.
 * By changing the @p $header and @p $rows_by_repo_id arguments,
 * the repository list can be customized accordingly.
 *
 * @param $vcs
 *   The unique string identifier for the version control system that
 *   the passed repository list covers.
 * @param $repositories
 *   An array of repositories of the given version control system.
 *   Array keys are the repository ids, and array values are the
 *   repository arrays like returned from versioncontrol_get_repository().
 * @param $header
 *   A list of columns that will be passed to theme('table').
 * @param $rows_by_repo_id
 *   An array of existing table rows, with repository ids as array keys.
 *   Each row already includes the generic column values, and for each row
 *   there is a repository with the same repository id given in the
 *   @p $repositories parameter.
 */
function versioncontrol_svn_versioncontrol_alter_repository_list($vcs, $repositories, &$header, &$rows_by_repo_id) {
  if ($vcs != 'svn') {
    return;
  }
  $header[] = t('Update method');
  $header[] = t('Last updated');

  foreach ($rows_by_repo_id as $repo_id => $row) {
    if ($repositories[$repo_id]['svn_specific']['update_method'] == VERSIONCONTROL_SVN_UPDATE_XSVN) {
      $rows_by_repo_id[$repo_id][] = t('external script');
      $rows_by_repo_id[$repo_id][] = t('n/a');
    }
    else if ($repositories[$repo_id]['svn_specific']['update_method'] == VERSIONCONTROL_SVN_UPDATE_CRON) {
      $rows_by_repo_id[$repo_id][] = t('logs (!fetch)', array(
        '!fetch' => l(t('fetch now'), 'admin/project/versioncontrol-repositories/update/svn/'. $repo_id)
      ));
      $rows_by_repo_id[$repo_id][] = $repositories[$repo_id]['svn_specific']['updated']
        ? t('!date (r!revision)', array(
             '!date' => format_date($repositories[$repo_id]['svn_specific']['updated'], 'small'),
             '!revision' => $repositories[$repo_id]['svn_specific']['last_revision'],
           ))
        : t('never');
    }
  }
}

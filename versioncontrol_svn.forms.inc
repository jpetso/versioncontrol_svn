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

define('VERSIONCONTROL_SVN_MIN_PASSWORD_LENGTH', 5);

/**
 * Implementation of hook_form_alter(): Add elements to various
 * administrative forms that the Version Control API provides.
 */
function versioncontrol_svn_form_alter($form_id, &$form) {
  if ($form['#id'] == 'versioncontrol-repository-form' && $form['#vcs'] == 'svn') {
    versioncontrol_svn_repository_admin_form_alter($form_id, $form);
  }
  else if ($form['#id'] == 'versioncontrol-account-form' && $form['#vcs'] == 'svn') {
    versioncontrol_svn_account_form_alter($form_id, $form);
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
    'The path of this repository. Example: /path/to/repository'
  );
  $form['repository_information']['update_method'] = array(
    '#type' => 'radios',
    '#title' => t('Update method'),
    '#description' => t('Automatic log retrieval requires cron.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['update_method']
                        : VERSIONCONTROL_SVN_UPDATE_XSVN,
    '#weight' => 10,
    '#options' => array(
      // log retrieval is not yet ported from subversion.module
      // VERSIONCONTROL_SVN_UPDATE_CRON => t('Automatic log retrieval.'),
      VERSIONCONTROL_SVN_UPDATE_XSVN => t('Use external script to insert data.'),
    ),
  );
  $form['repository_information']['allowed_serving_methods'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Serving methods'),
    '#description' => t('Check the serving methods that are being used for this repository. For new Subversion accounts, one of these methods will be assigned to the account.'),
    '#default_value' => versioncontrol_svn_get_allowed_serving_methods($repository),
    '#options' => array(
      VERSIONCONTROL_SVN_SERVER_APACHE_WEBDAV => t('Apache with mod_dav_svn'),
      VERSIONCONTROL_SVN_SERVER_SVNSERVE => t('Plain svnserve'),
      VERSIONCONTROL_SVN_SERVER_SVNSERVE_SSH => t('svnserve over SSH'),
    ),
    '#required' => TRUE,
    '#weight' => 12,
 );

  $form['svn_repository_layout'] = array(
    '#type' => 'fieldset',
    '#title' => t('Repository layout'),
    '#description' => t('In order to recognize branches and tags, the Subversion backend needs to know where the trunk, branches and tags directories are located in this repository.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 2,
  );
  $form['svn_repository_layout']['path_trunk'] = array(
    '#type' => 'textfield',
    '#title' => t('Trunk directory'),
    '#description' => t('Specify the path of the trunk directory here. Use %1 as a placeholder for a directory name (probably the project name) that also occurs in the branches and tags paths.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['path_trunk']
                        : '/%1/trunk',
    '#weight' => 0,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['svn_repository_layout']['path_branches'] = array(
    '#type' => 'textfield',
    '#title' => t('Branches directory'),
    '#description' => t('Specify the path of the branches directory here. Use %branch as a placeholder for the branch name and %1 for the directory name that was specified in the trunk directory.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['path_branches']
                        : '/%1/branches/%branch',
    '#weight' => 1,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['svn_repository_layout']['path_tags'] = array(
    '#type' => 'textfield',
    '#title' => t('Tags directory'),
    '#description' => t('Specify the path of the tags directory here. Use %tags as a placeholder for the tag name, %branch for the branch name and %1 for the directory name that was specified in the trunk directory.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['path_tags']
                        : '/%1/tags/%branch/%tag',
    '#weight' => 2,
    '#size' => 40,
    '#maxlength' => 255,
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

  $allowed_serving_methods = 0;
  foreach (array_filter($form_values['allowed_serving_methods']) as $type) {
    $allowed_serving_methods |= $type;
  }
  return array(
    'svn_specific' => array(
      'update_method' => $form_values['update_method'],
      'updated'       => $form_values['updated'],
      'last_revision' => $form_values['last_revision'],
      'path_trunk'    => $form_values['path_trunk'],
      'path_branches' => $form_values['path_branches'],
      'path_tags'     => $form_values['path_tags'],
      'allowed_serving_methods' => $allowed_serving_methods,
    ),
  );
}

<?php
// $Id$

/**
 * @file
 * Helper methods for hook test cases.
 *
 * Takes care of the setup and teardown of functionality common to all of the
 * repository hooks.
 */

class SvnHelper extends DrupalWebTestCase {

  /**
   * Root of the temporary SimpleTest files directory.
   *
   * All temporary files are written here.
   */
  protected $file_path;

  /**
   * The old working directory, before chdir().
   */
  protected $old_wd;

  /**
   * The full path to the root of the Drupal installation.
   */
  protected $base_path;

  /**
   * The OS-level user running the web site.
   */
  protected $server_user;

  /**
   * Array containing information about the repository and working copy.
   */
  protected $repo = array();

  /**
   * Array of hook info. The key is the hook name and the value is an array with
   * the following elements:
   *
   *   - src - The source of the xsvn script implementing the hook.
   *   - dst - The destination path of the xsvn script.
   *   - svn_file - The path of the svn hook which calls the xsvn script.
   *   - text - The source of the svn hook.
   *
   *   array(
   *     'name' => 'path',
   *   );
   */
  protected $hooks = array();

  /**
   * An array of the hooks provided.
   */
  protected $hook_names = array('pre-commit', 'post-commit');

  /**
   * Help set up a test to use Subversion.
   */
  function setUp() {
    parent::setUp('versioncontrol', 'versioncontrol_svn', 'commitlog');

    $this->old_wd          = getcwd();
    $this->file_path       = realpath(file_directory_path());
    $this->repo['path']    = $this->file_path .'/svnrepo';
    $this->repo['xsvn']    = $this->repo['path'] .'/hooks/xsvn';
    $this->repo['url']     = 'file://'. $this->file_path .'/svnrepo';
    $this->repo['wc_path'] = $this->file_path .'/svnwc';
    $this->repo['created'] = FALSE;
    $this->base_path       = dirname($_SERVER['SCRIPT_FILENAME']);
    $this->server_user     = $_ENV['APACHE_RUN_USER']; // TODO: Make this more portable
    $this->module_source   = realpath(drupal_get_path('module', 'versioncontrol_svn'));

    // The config 'hook' is special, so fill it in separately.
    $this->hooks['config']['src'] = $this->module_source .'/xsvn/xsvn-config.php';
    $this->hooks['config']['dst'] = $this->repo['xsvn'] .'/xsvn-config.php';
    $format = "\$xsvn['drupal_path'] = '%s';\n".
              "\$GLOBALS['simpletest_db_prefix'] = '%s';";

    $this->hooks['config']['text'] = sprintf($format, $this->base_path, $GLOBALS['db_prefix']);

    // Fill in each of the hooks.
    foreach ($this->hook_names as $hook) {
      $this->_fillHook($hook);
    }
  }

  /**
   * Fills in the information array for a single hook.
   *
   * @param $hook
   *   The name of the hook for which to fill in the info.
   */
  protected function _fillHook($hook) {
    $this->hooks[$hook] = array(
      'svn_file' => sprintf('%s/hooks/%s', $this->repo['path'], $hook),
      'src'      => sprintf('%s/xsvn/xsvn-%s.php', $this->module_source, $hook),
      'dst'      => sprintf('%s/xsvn-%s.php', $this->repo['xsvn'], $hook),
    );

    $dst = $this->hooks[$hook]['dst'];
    $config = $this->hooks['config']['dst'];
    $format = "#!/bin/sh\n".
              "export XDEBUG_CONFIG='remote_port=9001'\n".
              'php %s %s $@';
    $this->hooks[$hook]['text'] = sprintf($format, $dst, $config);

    return;
  }

  /**
   * Return to old working directory and clean up directories.
   */
  function tearDown() {
    chdir($this->old_wd);
    parent::tearDown();
  }

  /**
   * Create the subversion repository.
   */
  function initRepo() {
    shell_exec('svnadmin create '. $this->repo['path']);
    mkdir($this->repo['xsvn']);
    mkdir($this->repo['wc_path']);

    copy($this->hooks['config']['src'], $this->hooks['config']['dst']);

    // Append simpletest-specific info to the config file.
    $file = fopen($this->hooks['config']['dst'], 'a');
    fwrite($file, $this->hooks['config']['text']);
    fclose($file);
    $this->repo['created'] = TRUE;
    return;
  }

  /**
   * Copy the named hook to the repository directory. Throws an exception if the
   * repository is not already initialized.
   *
   * @param $hook
   *   The name of the hook to initialize.
   *
   * @return
   *   Null.
   */
  function initHook($hook) {
    if (!$this->repo['created']) {
      throw new Exception(t('Repository has not yet been created.'));
    }

    // Copy the xsvn files
    copy($this->hooks[$hook]['src'], $this->hooks[$hook]['dst']);

    $file = fopen($this->hooks[$hook]['svn_file'], 'w');
    fwrite($file, $this->hooks[$hook]['text']);
    fclose($file);
    chmod($this->hooks[$hook]['svn_file'], 0755);

    return;
  }

  /**
   * Initialize the repository in the Drupal database.
   *
   * @param $name
   *   The title to give the repository.
   *
   * @return
   *   The repository object.
   */
  function initDrupalRepo() {
    $menu_path = 'admin/project/versioncontrol-repositories/add-svn';
    $this->repo['name'] = $this->randomName();

    $edit = array();
    $edit['repo_name'] = $this->repo['name'];
    $edit['root'] = $this->repo['url'];
    $edit['update_method'] = VERSIONCONTROL_SVN_UPDATE_XSVN;
    $this->drupalPost($menu_path, $edit, t('Save repository'));
    $repo = array_shift(versioncontrol_get_repositories(array(
                'names' => array($edit['repo_name'])
              )));
    $this->repo['id'] = $repo['repo_id'];

    return $repo;
  }

  /**
   * Create an SVN account for the user with the given uid.
   */
  function initSvnAccount($uid, $username) {
    $menu_path = 'versioncontrol/register/'. $uid .'/'. $this->repo['id'];
    $edit = array();
    $edit['account_name'] = $username;

    $this->drupalPost($menu_path, $edit, t('Create Subversion account'));
  }

  /**
   * Check out a working copy of the repository.
   *
   * This should be verified to work in a test.
   */
  function wcCheckOut() {
    chdir($this->file_path);
    $format = 'svn checkout %s %s';
    return shell_exec(sprintf($format, $this->repo['url'], $this->repo['wc_path']));
  }

  /**
   * Adds a sample file to the working copy.
   *
   * Does not perform any checks for consistency.
   */
  function wcAddSampleFile($path='source.txt', $contents='file contents') {
    chdir($this->repo['wc_path']);
    $file = fopen($path, 'w');
    fwrite($file, $contents);
    fclose($file);

    return shell_exec("svn add $path");
  }

  /**
   * Run 'svn commit' with the message given.
   */
  function wcCommit($msg = 'commit message') {
    return shell_exec("svn commit -m '$msg' 2>&1");
  }

  /**
   * Make sure the given operation appears in the commit log on the current
   * page.
   *
   * @param $operation
   *   The operation for which to check.
   */
  function assertOperationLog($op_id, $operation) {
    $options = array('query' => array('id' => $op_id));
    $this->drupalGet('commitlog', $options);

    $this->assertResponse(
      200,
      t('User can access commit log.')
    );

    $this->assertNoText(
      t('No log messages found.'),
      t('Make sure a log message is found.'),
      t('operation log')
    );

    $this->assertText(
      $operation['message'],
      t('Make sure page message matches operation message.'),
      t('operation log')
    );

    $theme_string = theme('commitlog_operations_page', array(
              'vc_op_ids' => array($op_id)
            ));

    // For some reason, the closing tag of the outer <div> appears on the same
    // line in $page, but not in the content of the page. It doesn't seem like
    // that should happen. Convert both to XML and then cast them to strings.
    $theme_xml = new SimpleXMLElement($theme_string);
    $returned = array_shift($this->elements->xpath('//div[@class="commitlog"]'));

    $this->assertEqual(
      (string) $returned,
      (string) $theme_xml,
      t('Found a themed page with the given commit.'),
      t('operation log')
    );
  }
}

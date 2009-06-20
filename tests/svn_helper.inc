<?php

class SvnHelper extends DrupalWebTestCase {

  /**
   * Root of the temporary SimpleTest files directory.
   *
   * All temporary files are written here.
   */
  protected $file_path;

  /**
   * Full path to the temporary SVN repository.
   */
  protected $repo_path;

  /**
   * URL of the temporary SVN repository.
   */
  protected $repo_url;

  /**
   * Full path of the temporary SVN working copy.
   */
  protected $wc_path;

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
   * The configuration extension to apply to the copied xsvn-config.php file.
   *
   * The config files must be customized for each site, so an addition to the
   * copied file is needed.
   */
  protected $config_ext;

  /**
   * Full path to the pre-commit hook in the Subversion "hooks" directory.
   */
  protected $pre_commit_hook;

  /**
   * Full path to the source php pre-commit hook.
   */
  protected $pre_commit_file_src;

  /**
   * Full path to the source xsvn-config.php file.
   */
  protected $xsvn_config_src;

  /**
   * Full path to the "xsvn" directory in the temporary Subversion repository.
   */
  protected $xsvn_dir;

  /**
   * Destination of the pre-commit php file.
   */
  protected $pre_commit_file_dst;

  /**
   * Destination of the xsvn-config.php file.
   */
  protected $xsvn_config_dst;

  /**
   * The text of the pre-commit hook file.
   *
   * This is for the pre-commit hook to call the php file, which then does the
   * heavy lifting.
   */
  protected $pre_commit_text;

  /**
   * Help set up a test to use Subversion.
   */
  function setUp() {
    parent::setUp('versioncontrol', 'versioncontrol_svn');

    $this->file_path = realpath(file_directory_path());
    $this->repo_path = $this->file_path . '/svnrepo';
    $this->repo_url = 'file://' . $this->file_path . '/svnrepo';
    $this->wc_path = $this->file_path . '/svnwc';
    $this->old_wd = getcwd();
    $this->base_path = dirname($_SERVER['SCRIPT_FILENAME']);
    $this->server_user = $_ENV['APACHE_RUN_USER'];

    $this->config_ext = "\$xsvn['drupal_path'] = '" . $this->base_path . "';\n";
    $this->pre_commit_hook = $this->repo_path . '/hooks/pre-commit';

    $module_source = realpath(drupal_get_path('module', 'versioncontrol_svn'));
    $this->pre_commit_file_src = $module_source . '/xsvn/xsvn-pre-commit.php';
    $this->xsvn_config_src = $module_source . '/xsvn/xsvn-config.php';

    $this->xsvn_dir = $this->repo_path . '/hooks/xsvn';
    $this->pre_commit_file_dst = $this->xsvn_dir . '/xsvn-pre-commit.php';
    $this->xsvn_config_dst = $this->xsvn_dir . '/xsvn-config.php';

    $this->pre_commit_text = "#!/bin/sh\n".
      "$this->pre_commit_file_dst $this->xsvn_config_dst " .
      '$@';

    // Create subversion repository
    shell_exec("svnadmin create $this->repo_path");

    // Write the pre-commit hook
    $file = fopen($this->pre_commit_hook, 'w');
    fwrite($file, $this->pre_commit_text);
    fclose($file);
    chmod($this->pre_commit_hook, 0755);

    mkdir($this->xsvn_dir);
    mkdir($this->wc_path);

    // Copy the xsvn files
    copy($this->pre_commit_file_src, $this->pre_commit_file_dst);
    copy($this->xsvn_config_src, $this->xsvn_config_dst);
    chmod($this->pre_commit_file_dst, 0755);

    // Write the correct base_path to the config file.
    $file = fopen($this->xsvn_config_dst, 'a');
    fwrite($file, $this->config_ext);
    fclose($file);
  }

  /**
   * Return to old working directory.
   */
  function tearDown() {
    chdir($this->old_wd);
    parent::tearDown();
  }

}

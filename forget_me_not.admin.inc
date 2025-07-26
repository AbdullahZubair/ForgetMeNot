<?php

/**
 * @file
 * Administrative interface functions for ForgetMeNot module.
 */

/**
 * Main administration page callback.
 * 
 * Displays currently excluded projects with management options.
 * 
 * @return string
 *   Rendered HTML for the administration page.
 */
function forget_me_not_admin_page() {
  $excluded_projects = forget_me_not_get_excluded_projects();
  
  $build = array();
  
  // Set the page title.
  drupal_set_title(t('Manage Excluded Projects'));
  $build['description'] = array(
    '#type' => 'item',
    '#markup' => '<div class="forget-me-not-help">' .
      t('View and manage which projects are excluded from update status checks. Excluded projects will not appear in update notifications.') .
      '</div>',
  );
  
  // Display excluded projects or empty state.
  $projects_by_type = forget_me_not_group_projects_by_type($excluded_projects);
  $sections = array();
  foreach ($projects_by_type as $type => $projects) {
    if (empty($projects)) {
      continue;
    }
    $sections[$type] = array(
      '#type' => 'fieldset',
      '#title' => ucfirst($type),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      '#attributes' => array('class' => array('forget-me-not-project-type')),
      'list' => array(
        '#theme' => 'item_list',
        '#items' => forget_me_not_build_project_list_items($projects),
        '#attributes' => array(
          'class' => array('forget-me-not-project-list'),
          'id' => 'excluded-projects-list',
        ),
      ),
    );
  }
  if (!empty($sections)) {
    $build['excluded_projects'] = array(
      '#type' => 'fieldset',
      '#title' => t('Currently Excluded Projects'),
      '#description' => t('These projects are excluded from update checks. Click "Remove" to re-enable update checking for a project.'),
      '#collapsible' => FALSE,
      '#attributes' => array('class' => array('forget-me-not-excluded-projects')),
    ) + $sections;
  } else {
    $build['no_projects'] = array(
      '#type' => 'item',
      '#markup' => '<div class="forget-me-not-empty-state">' .
        '<p>' . t('No projects have been excluded yet.') . '</p>' .
        '<p>' . t('Use the "Select Projects to Exclude" button above to get started.') . '</p>' .
        '</div>',
    );
  }
  
  // Add status messages area for AJAX responses.
  $build['messages'] = array(
    '#type' => 'container',
    '#attributes' => array(
      'id' => 'forget-me-not-messages',
      'class' => array('forget-me-not-messages'),
      'aria-live' => 'polite',
    ),
  );
  
  return $build;
}

/**
 * Groups projects by their type for organized display.
 * 
 * @param array $projects
 *   Array of project machine names.
 * 
 * @return array
 *   Associative array grouped by project type.
 */
function forget_me_not_group_projects_by_type(array $projects) {
  $grouped = array(
    'core' => array(),
    'modules' => array(),
    'themes' => array(),
  );
  
  foreach ($projects as $project) {
    $type = forget_me_not_get_project_type($project);
    if ($type === 'module') {
      $grouped['modules'][] = $project;
    }
    elseif ($type === 'theme') {
      $grouped['themes'][] = $project;
    }
    elseif ($type === 'core') {
      $grouped['core'][] = $project;
    }
  }
  
  return $grouped;
}

/**
 * Builds list items for project display.
 * 
 * @param array $projects
 *   Array of project machine names.
 * 
 * @return array
 *   Array of list items for theme_item_list().
 */
function forget_me_not_build_project_list_items(array $projects) {
  $items = array();
  
  foreach ($projects as $project) {
    $type = forget_me_not_get_project_type($project);
    $display_name = forget_me_not_get_project_display_name($project);
    
    $remove_button = array(
      '#type' => 'button',
      '#value' => t('Remove'),
      '#attributes' => array(
        'class' => array('button', 'button-small', 'remove-project'),
        'data-project' => $project,
        'title' => t('Remove @project from exclusion list', array('@project' => $display_name)),
      ),
    );
    
    $items[] = array(
      'data' => '<span class="project-name">' . check_plain($display_name) . '</span> ' .
                '<span class="project-type">(' . ucfirst($type) . ')</span> ' .
                drupal_render($remove_button),
      'id' => 'project-' . check_plain($project),
      'class' => array('forget-me-not-project-item', 'project-type-' . $type),
    );
  }
  
  return $items;
}

/**
 * Gets the display name for a project.
 * 
 * @param string $project_name
 *   The machine name of the project.
 * 
 * @return string
 *   The human-readable project name.
 */
function forget_me_not_get_project_display_name($project_name) {
  if ($project_name === 'drupal') {
    return t('Drupal Core');
  }
  
  // Try to get the human name from module info.
  $modules = system_rebuild_module_data();
  if (isset($modules[$project_name])) {
    return $modules[$project_name]->info['name'];
  }
  
  // Try to get the human name from theme info.
  $themes = list_themes();
  if (isset($themes[$project_name])) {
    return $themes[$project_name]->info['name'];
  }
  
  // Fallback to machine name.
  return $project_name;
}

/**
 * Form builder for project selection.
 * 
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 * 
 * @return array
 *   The built form array.
 */
function forget_me_not_select_projects_form($form, &$form_state) {
  $excluded_projects = forget_me_not_get_excluded_projects();
  $available_projects = forget_me_not_get_available_projects($excluded_projects);
  $form['#tree'] = TRUE;
  // Set the page title for the selection page.
  drupal_set_title(t('Select Projects to Exclude'));
  $form['description'] = array(
    '#type' => 'item',
    '#markup' => '<div class="form-description">' .
      t('Select which modules, themes, or Drupal core to exclude from update status checks. Only enabled modules and themes are shown.') .
      '</div>',
  );
  // Use vertical tabs for organization.
  $form['projects'] = array(
    '#type' => 'vertical_tabs',
    '#title' => t('Project Categories'),
  );

  // Modules fieldset in vertical tabs.
  if (!empty($available_projects['modules'])) {
    $form['modules_section'] = array(
      '#type' => 'fieldset',
      '#title' => t('Modules (@count)', array('@count' => count($available_projects['modules']))),
      '#group' => 'projects',
    );
    $form['modules_section']['modules'] = array(
      '#type' => 'checkboxes',
      '#options' => $available_projects['modules'],
    );
  }

  // Themes fieldset in vertical tabs.
  if (!empty($available_projects['themes'])) {
    $form['themes_section'] = array(
      '#type' => 'fieldset',
      '#title' => t('Themes (@count)', array('@count' => count($available_projects['themes']))),
      '#group' => 'projects',
    );
    $form['themes_section']['themes'] = array(
      '#type' => 'checkboxes',
      '#options' => $available_projects['themes'],
    );
  }

  // Core fieldset in vertical tabs.
  if (!empty($available_projects['core'])) {
    $form['core_section'] = array(
      '#type' => 'fieldset',
      '#title' => t('Drupal Core'),
      '#group' => 'projects',
    );
    $form['core_section']['core'] = array(
      '#type' => 'checkboxes',
      '#options' => $available_projects['core'],
    );
  }

  // Actions.
  $form['actions'] = array(
    '#type' => 'actions',
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Exclude Selected Projects'),
    '#attributes' => array('class' => array('button-primary')),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => FORGET_ME_NOT_BASE_PATH,
    '#attributes' => array('class' => array('button')),
  );

  return $form;
}

/**
 * Validation handler for project selection form.
 * 
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function forget_me_not_select_projects_form_validate($form, &$form_state) {
  $selected_projects = array();
  
  // Collect all selected projects.
  foreach (array('modules', 'themes', 'core') as $type) {
    if (!empty($form_state['values'][$type . '_section'][$type])) {
      $selected = array_filter($form_state['values'][$type . '_section'][$type]);
      $selected_projects = array_merge($selected_projects, array_keys($selected));
    }
  }
  
  // Validate at least one project is selected.
  if (empty($selected_projects)) {
    form_set_error('projects', t('Please select at least one project to exclude.'));
    return;
  }
  
  // Validate each project name.
  foreach ($selected_projects as $project) {
    if (!forget_me_not_validate_project_name($project)) {
      form_set_error('projects', t('Invalid project name: @project', array('@project' => $project)));
    }
  }
  
  // Store selected projects for submit handler.
  $form_state['selected_projects'] = $selected_projects;
}

/**
 * Submit handler for project selection form.
 * 
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function forget_me_not_select_projects_form_submit($form, &$form_state) {
  $selected_projects = isset($form_state['selected_projects']) ? $form_state['selected_projects'] : array();

  if (!empty($selected_projects)) {
    // Get currently excluded projects and merge with new ones.
    $excluded_projects = forget_me_not_get_excluded_projects();
    $updated_projects = array_unique(array_merge($excluded_projects, $selected_projects));

    // Save the updated list.
    if (forget_me_not_save_excluded_projects($updated_projects)) {
      // Group by type for message.
      $grouped = forget_me_not_group_projects_by_type($selected_projects);
      $message = '';
      $total = count($selected_projects);
      $type_labels = array('modules' => 'Module', 'themes' => 'Theme', 'core' => 'Drupal Core');
      $type_labels_plural = array('modules' => 'Modules', 'themes' => 'Themes', 'core' => 'Drupal Core');
      $list_items = '';
      foreach ($grouped as $type => $projects) {
        if (empty($projects)) continue;
        if ($type === 'core') {
          $message .= t('Drupal core has been excluded from update checks.') . '<br>';
        } else {
          $label = count($projects) === 1 ? $type_labels[$type] : $type_labels_plural[$type];
          $message .= t('!count !type excluded:', array('!count' => count($projects), '!type' => $label));
          $message .= '<ul>';
          foreach ($projects as $project) {
            $message .= '<li>' . check_plain(forget_me_not_get_project_display_name($project)) . '</li>';
          }
          $message .= '</ul>';
        }
      }
      drupal_set_message($message, 'status', FALSE);
      // Log the action.
      watchdog('forget_me_not', 'User excluded @count projects: @projects', array(
        '@count' => $total,
        '@projects' => implode(', ', $selected_projects),
      ), WATCHDOG_INFO);
    }
    else {
      drupal_set_message(t('An error occurred while saving the excluded projects.'), 'error');
    }
  }

  // Redirect back to main page.
  $form_state['redirect'] = FORGET_ME_NOT_BASE_PATH;
}

/**
 * AJAX callback for removing a project from the exclusion list.
 * 
 * Handles the AJAX request to remove a single project from exclusions.
 */
function forget_me_not_ajax_remove_project() {
  // Validate request method.
  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid request method.')));
    drupal_exit();
  }
  
  // Validate and sanitize input.
  $project_name = isset($_POST['project']) ? trim($_POST['project']) : '';
  
  if (empty($project_name) || !forget_me_not_validate_project_name($project_name)) {
    drupal_json_output(array('status' => 'error', 'message' => t('Invalid project name.')));
    drupal_exit();
  }
  
  // Get current exclusions.
  $excluded_projects = forget_me_not_get_excluded_projects();
  
  // Remove the project if it exists.
  $key = array_search($project_name, $excluded_projects);
  if ($key !== FALSE) {
    unset($excluded_projects[$key]);
    
    // Save updated list.
    if (forget_me_not_save_excluded_projects($excluded_projects)) {
      // Log the removal.
      watchdog('forget_me_not', 'User removed project from exclusions: @project', array(
        '@project' => $project_name,
      ), WATCHDOG_INFO);
      
      drupal_json_output(array(
        'status' => 'success',
        'message' => t('Project "@project" has been removed from exclusions.', array('@project' => $project_name)),
      ));
    }
    else {
      drupal_json_output(array('status' => 'error', 'message' => t('Failed to save changes.')));
    }
  }
  else {
    drupal_json_output(array('status' => 'error', 'message' => t('Project not found in exclusions.')));
  }
  
  drupal_exit();
}

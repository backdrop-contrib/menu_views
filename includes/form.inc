<?php

/**
 * @file
 * Form hooks for the menu_views module.
 */

/**
 * Implements hook_form_FORM_ID_alter().
 * Appends the attached view to the title of the menu item.
 */
function menu_views_form_menu_overview_form_alter(&$form, &$form_state) {
  drupal_add_css(drupal_get_path('module', 'menu_views') . '/css/menu_views.css', array('group' => CSS_DEFAULT));
  $elements = element_children($form);
  foreach ($elements as $mlid) {
    $element = &$form[$mlid];
    // Only process menu items
    if (isset($element['#item'])) {
      $item = &$element['#item'];
      $menu_views = menu_views($item);
      // Only change the title for attached views
      if ($menu_views->name) {
        $view = views_get_view($menu_views->name);
        if (isset($view)) {
          $name = $view->human_name ? $view->human_name : $view->name;
          if ($menu_views->display && isset($view->display[$menu_views->display])) {
            $display = $view->display[$menu_views->display]->display_title;
            $arguments = $menu_views->tokenize_arguments(TRUE);
            if (!empty($arguments)) {
              $arguments = ' : ' . $arguments;
            }
            $disabled = $item['hidden'] ? ' (' . t('disabled') . ')' : '';
            $title = '';
            if ($item['link_path'] != '<view>') {
              // Must manually set the HTML here. Don't need the view showing up here by runnging it through l();
              $title = '<a href="' . check_plain(url($item['href'], $item['localized_options'])) . '"' . drupal_attributes($item['localized_options']['attributes']) . '>' . check_plain($item['title']) . '</a> ';
            }
            $element['title']['#markup'] = $title . '<div class="messages status view">Attached view: ( ' . $name . ' : ' . $display . $arguments . ' )</div>' . $disabled;
          }
        }
      }
    }
  }
}

/**
 * Implements hook_form_FORM_ID_alter().
 * Change path description. Insert the views selection.
 */
function menu_views_form_menu_edit_item_alter(&$form, &$form_state) {
  // Append the menu item's link path description.
  if (isset($form['link_path']['#description'])) {
    $form['link_path']['#description'] .= ' ' . t('Enter %view to disable the link and display only the view.', array('%view' => '<view>'));
  }
  
  // Unset the previous value so that the new values get saved.
  unset($form['options']['#value']['menu_views']);
  
  // Get the values for the current, default or new attached view.
  $menu_views = menu_views($form, $form_state);
  // Setup the form  
  $form['options']['menu_views'] = array(
    '#access' => user_access('administer menu views'),
    '#type' => 'fieldset',
    '#title' => t('Attach view'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
    '#prefix' => '<div id="menu-views">',
    '#suffix' => '</div>',
    '#tree' => TRUE,
    '#weight' => -1,
  );
  $options = array();
  $views = views_get_enabled_views();
  foreach ($views as $name => $view) {
    $options[$name] = $view->human_name;
  }
  ksort($options);
  $form['options']['menu_views']['name'] = array(
    '#type' => 'select',
    '#title' => t('View'),
    '#empty_option' => t('- None -'),
    '#description' => t('Select a view to attach.'),
    '#default_value' => $menu_views->name,
    '#options' => $options,
    '#ajax' => array(
      'callback' => 'menu_views_ajax_callback',
      'wrapper' => 'menu-views',
    ),
  );
  if ($menu_views->name) {
    $view = views_get_view($menu_views->name);
    $options = array();
    foreach ($view->display as $name => $display) {
      $options[$name] = $display->display_title;
    }
    $form['options']['menu_views']['display'] = array(
      '#type' => 'select',
      '#title' => t('Display'),
      '#description' => t('Select a view display to use.'),
      '#empty_option' => t('- Select a display -'),
      '#default_value' => $menu_views->display,
      '#options' => $options,
      '#ajax' => array(
        'callback' => 'menu_views_ajax_callback',
        'wrapper' => 'menu-views',
      ),
    );
  }
  if ($menu_views->display) {
    $form['options']['menu_views']['arguments'] = array(
      '#type' => 'textfield',
      '#title' => t('Arguments'),
      '#description' => t('You can reference the node associated with this menu link by using <code>[menu-link:node:nid]</code>.<br />You can reference the node associated with the parent of this menu link by using <code>[menu-link:parent:node:nid]</code>.'),
      '#default_value' => $menu_views->arguments,
    );
    if (module_exists('token')) {
      $form['options']['menu_views']['tokens'] = array(
        '#type' => 'fieldset',
        '#title' => t('Replacement Tokens'),
        '#description' => t('Below are the list of available tokens to use for passing arguments to this attached view.<br /><br />You can reference the node associated with this menu link by using <code>[menu-link:node:nid]</code>.<br />You can reference the node associated with the parent of this menu link by using <code>[menu-link:parent:node:nid]</code>.'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      $form['options']['menu_views']['tokens']['tokens'] = array(
        '#theme' => 'token_tree',
        '#token_types' => array('menu-link'),
      );
    }
  }
  // Inject handlers
  $form['#validate'] = array_merge(array('menu_views_menu_edit_item_validate'), $form['#validate']);
  $form['#submit'] = array_merge(array('menu_views_menu_edit_item_submit'), $form['#submit']);
}

/**
 * Ajax callback for re-rendering the attach views portion of the form.
 */
function menu_views_ajax_callback($form, $form_state) {
  return $form['options']['menu_views'];
}

/**
 * Validate handler for menu_edit_item form
 */
function menu_views_menu_edit_item_validate($form, &$form_state) {
  // Only run this validation when the form is fully submitted.
  if ($form_state['submitted']) {
    if ($form_state['values']['link_path'] == '<view>' && $form_state['values']['menu_views']['name'] == '') {
      form_set_error('menu_views][name', t('The link path has been set to %view. You must select a view to attach to this menu item.', array('%view' => '<view>')));
    }
  }
}

/**
 * Submit handler for menu_edit_item form
 */
function menu_views_menu_edit_item_submit($form, &$form_state) {
  $view = menu_views($form, $form_state);
  $menu_views = array(
    'name' => $view->name,
    'display' => $view->display,
    'arguments' => $view->arguments,
    // Force the original mlid to be saved.
    'mlid' => $form_state['values']['original_item']['mlid'],
  );
  $form_state['values']['options']['menu_views'] = $menu_views;
  if (isset($form_state['values']['menu_views'])) {
    unset($form_state['values']['menu_views']);
  }
}

/*
 * JS for ForgetMeNot admin UI.
 * Handles project exclusion/removal and user feedback.
 */

(function ($, Drupal) {
  'use strict';

  Drupal.behaviors.forgetMeNot = {
    attach: function (context, settings) {
      var self = this;
      self.initMessageSystem();
      $('.remove-project', context).once('forget-me-not-remove').each(function () {
        self.initRemoveButton($(this));
      });
    },

    // Message system for status/error feedback
    initMessageSystem: function () {
      this.messageContainer = $('#forget-me-not-messages');
      if (this.messageContainer.length === 0) {
        this.messageContainer = $('<div id="forget-me-not-messages" class="forget-me-not-messages" aria-live="polite"></div>');
        $('.forget-me-not-excluded-projects').before(this.messageContainer);
      }
    },

    showMessage: function (type, message, autoHide) {
      var self = this;
      autoHide = autoHide !== false;
      this.clearMessages();
      var messageElement = $('<div class="messages ' + type + '" role="alert">')
        .html('<span class="message-text">' + Drupal.t(message) + '</span>')
        .hide()
        .appendTo(this.messageContainer)
        .fadeIn(300);
      if (autoHide && type === 'status') {
        setTimeout(function () {
          self.hideMessage(messageElement);
        }, 5000);
      }
      messageElement.attr('tabindex', -1).focus();
    },

    hideMessage: function (messageElement) {
      messageElement.fadeOut(300, function () {
        $(this).remove();
      });
    },

    clearMessages: function () {
      this.messageContainer.find('.messages').fadeOut(200, function () {
        $(this).remove();
      });
    },

    // Set up remove button handlers
    initRemoveButton: function (button) {
      var self = this;
      button.on('click', function (e) {
        e.preventDefault();
        self.handleProjectRemoval($(this));
      });
      button.on('keydown', function (e) {
        if (e.which === 13 || e.which === 32) {
          e.preventDefault();
          self.handleProjectRemoval($(this));
        }
      });
    },

    // AJAX project removal
    handleProjectRemoval: function (button) {
      var self = this;
      var projectName = button.data('project');
      var projectItem = button.closest('.forget-me-not-project-item');
      var displayName = projectItem.find('.project-name').text() || projectName;
      if (!projectName || typeof projectName !== 'string') {
        self.showMessage('error', 'Invalid project name.', false);
        return;
      }
      var confirmMessage = Drupal.t('Are you sure you want to remove "@project" from the exclusion list? This will re-enable update notifications for this project.', {
        '@project': displayName
      });
      if (!confirm(confirmMessage)) {
        return;
      }
      self.setLoadingState(button, true);
      projectItem.addClass('forget-me-not-loading');
      $('.remove-project').prop('disabled', true);
      $.ajax({
        url: Drupal.settings.basePath + 'admin/config/system/forget_me_not/remove',
        type: 'POST',
        data: { project: projectName },
        dataType: 'json',
        timeout: 10000,
        success: function (response) {
          self.handleRemovalSuccess(response, projectItem, displayName);
        },
        error: function (xhr, status, error) {
          self.handleRemovalError(xhr, status, error, projectItem);
        },
        complete: function () {
          $('.remove-project').prop('disabled', false);
          self.setLoadingState(button, false);
          projectItem.removeClass('forget-me-not-loading');
        }
      });
    },

    handleRemovalSuccess: function (response, projectItem, displayName) {
      var self = this;
      if (response.status === 'success') {
        projectItem.addClass('removing').fadeOut(400, function () {
          var $fieldset = projectItem.closest('fieldset');
          $(this).remove();
          if ($fieldset.find('.forget-me-not-project-item').length === 0) {
            $fieldset.remove();
          }
          if ($('.forget-me-not-excluded-projects .forget-me-not-project-item').length === 0) {
            self.showEmptyState();
          }
        });
        var message = response.message || Drupal.t('Project "@name" has been removed from exclusions.', {
          '@name': displayName
        });
        self.showMessage('status', message);
      } else {
        self.showMessage('error', response.message || Drupal.t('An unexpected error occurred.'), false);
      }
    },

    handleRemovalError: function (xhr, status, error, projectItem) {
      var message;
      switch (status) {
        case 'timeout':
          message = 'The request timed out. Please check your connection and try again.';
          break;
        case 'abort':
          message = 'The request was cancelled.';
          break;
        case 'parsererror':
          message = 'Invalid response from server.';
          break;
        default:
          if (xhr.status === 403) {
            message = 'You do not have permission to perform this action.';
          } else if (xhr.status === 404) {
            message = 'The requested resource was not found.';
          } else if (xhr.status >= 500) {
            message = 'A server error occurred. Please try again later.';
          } else {
            message = 'An error occurred while removing the project. Please try again.';
          }
      }
      this.showMessage('error', message, false);
    },

    setLoadingState: function (button, loading) {
      if (loading) {
        button.data('original-text', button.val())
              .val(Drupal.t('Removing...'))
              .addClass('loading');
      } else {
        button.val(button.data('original-text') || Drupal.t('Remove'))
              .removeClass('loading');
      }
    },

    showEmptyState: function () {
      var emptyStateHtml = '<div class="forget-me-not-empty-state">' +
        '<p>' + Drupal.t('No projects have been excluded yet.') + '</p>' +
        '<p>' + Drupal.t('Use the "Select Projects to Exclude" button above to get started.') + '</p>' +
        '</div>';
      $('.forget-me-not-excluded-projects').replaceWith(emptyStateHtml);
    }
  };

})(jQuery, Drupal);
